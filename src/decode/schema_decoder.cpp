#include "decode/schema_decoder.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/field_codec.hpp"
#include "observability/logging.hpp"
#include "observability/metrics.hpp"

#include <QString>
#include <algorithm>
#include <chrono>
#include <utility>

namespace signalforge::decoder {

namespace {

using signalforge::observability::Metric;
using signalforge::observability::MetricKind;
using signalforge::observability::MetricsRegistry;

// Byte-reading + encoding-classification primitives are shared with the
// UI-side FrameDissector — see decode/field_codec.hpp.
using namespace signalforge::decoder::codec;

QString registerMetricName(const QString& base, const QString& driverId) {
    return QStringLiteral("decoder_%1_%2").arg(base, driverId);
}

Metric* registerMetric(const QString& base, const QString& driverId, MetricKind kind) {
    return MetricsRegistry::instance().getOrCreate(registerMetricName(base, driverId), kind);
}

QString signalIdFor(const QString& driverId, const QString& fieldName) {
    return QStringLiteral("%1/%2").arg(driverId, fieldName);
}

QString signalIdFor(const QString& driverId, const QString& fieldName, const QString& bitName) {
    return QStringLiteral("%1/%2/%3").arg(driverId, fieldName, bitName);
}

QString hexPrefix(const QByteArray& payload, int maxBytes = 16) {
    const int n = std::min<int>(maxBytes, static_cast<int>(payload.size()));
    QString out;
    out.reserve(n * 3);
    for (int i = 0; i < n; ++i) {
        out += QStringLiteral("%1").arg(static_cast<unsigned char>(payload.at(i)), 2, 16, QLatin1Char('0'));
        if (i + 1 < n) {
            out += QLatin1Char(' ');
        }
    }
    return out;
}

}  // namespace

SchemaDecoder::SchemaDecoder(Schema schema, QString driverId)
    : schema_(std::move(schema)), driverId_(std::move(driverId)) {
    mDecoded_ = registerMetric(QStringLiteral("frames_decoded"), driverId_, MetricKind::Counter);
    mUnmatched_ = registerMetric(QStringLiteral("frames_unmatched"), driverId_, MetricKind::Counter);
    mMalformed_ = registerMetric(QStringLiteral("frames_malformed"), driverId_, MetricKind::Counter);
    mEmitted_ = registerMetric(QStringLiteral("signals_emitted"), driverId_, MetricKind::Counter);
    mLastDecodeUs_ = registerMetric(QStringLiteral("last_decode_us"), driverId_, MetricKind::Gauge);

    layoutCache_.reserve(schema_.layouts.size());
    for (const Layout& layout : schema_.layouts) {
        CachedLayout cached;
        cached.fields.reserve(layout.fields.size());
        for (const FieldDef& field : layout.fields) {
            CachedField cf;
            if (field.encoding == FieldEncoding::BitField) {
                cf.bitFieldMetaIndices.reserve(field.bitFields.size());
                for (const BitFieldDef& bit : field.bitFields) {
                    SignalMetadata meta;
                    meta.id = signalIdFor(driverId_, field.name, bit.name);
                    meta.name = bit.name;
                    meta.unit = field.unit;
                    meta.type = (bit.bitCount == 1) ? SignalType::Bool : SignalType::Int64;
                    meta.description = bit.description;
                    cf.bitFieldMetaIndices.push_back(static_cast<int>(metadataCatalog_.size()));
                    metadataCatalog_.push_back(std::move(meta));
                }
            } else if (field.encoding == FieldEncoding::FixedString) {
                SignalMetadata meta;
                meta.id = signalIdFor(driverId_, field.name);
                meta.name = field.name;
                meta.unit = field.unit;
                meta.type = SignalType::String;
                meta.description = field.description;
                cf.metaIndex = static_cast<int>(metadataCatalog_.size());
                metadataCatalog_.push_back(std::move(meta));
            } else if (encodingIsNumeric(field.encoding)) {
                SignalMetadata meta;
                meta.id = signalIdFor(driverId_, field.name);
                meta.name = field.name;
                meta.unit = field.unit;
                const bool transformed = field.scale.has_value() || field.offsetTransform.has_value();
                if (encodingIsFloat(field.encoding) || transformed) {
                    meta.type = SignalType::Double;
                } else {
                    meta.type = SignalType::Int64;
                }
                meta.description = field.description;
                meta.scale = field.scale;
                meta.offset = field.offsetTransform;
                cf.metaIndex = static_cast<int>(metadataCatalog_.size());
                metadataCatalog_.push_back(std::move(meta));
            }
            cached.fields.push_back(std::move(cf));
        }
        layoutCache_.push_back(std::move(cached));
    }
}

SchemaDecoder::~SchemaDecoder() {
    std::shared_ptr<SignalValueSink> sink;
    {
        std::lock_guard lock(sinkMutex_);
        sink = sink_;
    }
    if (sink) {
        try {
            sink->onSignalsUnregistered(driverId_);
        } catch (const std::exception& e) {
            SF_LOG_ERROR("SchemaDecoder[{}]: sink threw on unregister: {}", driverId_.toStdString(), e.what());
        }
    }
}

QString SchemaDecoder::schemaId() const {
    return schema_.id;
}

std::vector<SignalMetadata> SchemaDecoder::signalMetadata() const {
    return metadataCatalog_;
}

void SchemaDecoder::setSignalSink(std::shared_ptr<SignalValueSink> sink) {
    std::shared_ptr<SignalValueSink> previous;
    {
        std::lock_guard lock(sinkMutex_);
        if (sink_ == sink) {
            SF_LOG_WARN("SchemaDecoder[{}]: setSignalSink called twice with the same pointer; ignoring",
                        driverId_.toStdString());
            return;
        }
        previous = sink_;
        sink_ = sink;
        bufferCache_.reset();  // invalidate the M6 fast-path cache; rebuilt below if applicable
    }
    if (previous) {
        try {
            previous->onSignalsUnregistered(driverId_);
        } catch (const std::exception& e) {
            SF_LOG_ERROR("SchemaDecoder[{}]: previous sink threw on unregister: {}", driverId_.toStdString(), e.what());
        }
    }
    if (sink) {
        try {
            sink->onSignalsRegistered(driverId_, metadataCatalog_);
        } catch (const std::exception& e) {
            SF_LOG_ERROR("SchemaDecoder[{}]: sink threw on register: {}", driverId_.toStdString(), e.what());
        }

        // M6 fast-path: if the sink is a SignalBufferRegistry, the registry
        // has now allocated a SignalBuffer per metadata signal. Cache the
        // SignalBuffer* pointers indexed by metadata catalog position so the
        // decoder's hot loop can push directly without touching the
        // registry's mutex or QString-keyed map.
        if (auto* registry = dynamic_cast<signalforge::buffer::SignalBufferRegistry*>(sink.get())) {
            auto cache =
                std::make_shared<std::vector<signalforge::buffer::SignalBuffer*>>(metadataCatalog_.size(), nullptr);
            for (std::size_t i = 0; i < metadataCatalog_.size(); ++i) {
                (*cache)[i] = registry->bufferFor(metadataCatalog_[i].id);
            }
            std::lock_guard lock(sinkMutex_);
            bufferCache_ = std::move(cache);
        }
    }
}

QString SchemaDecoder::sinkName() const {
    return QStringLiteral("SchemaDecoder(%1)").arg(driverId_);
}

void SchemaDecoder::onFrame(const signalforge::frame::RawFrame& frame) {
    const auto t0 = std::chrono::steady_clock::now();

    std::shared_ptr<SignalValueSink> sink;
    std::shared_ptr<const std::vector<signalforge::buffer::SignalBuffer*>> cache;
    {
        std::lock_guard lock(sinkMutex_);
        sink = sink_;
        cache = bufferCache_;
    }

    const bool matched = tryDecodeFrame(frame, std::move(sink), std::move(cache));

    const auto t1 = std::chrono::steady_clock::now();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    if (mLastDecodeUs_ != nullptr) {
        mLastDecodeUs_->set(static_cast<std::int64_t>(us));
    }

    if (!matched) {
        if (mUnmatched_ != nullptr) {
            mUnmatched_->add(1);
        }
        SF_LOG_WARN("SchemaDecoder[{}]: no layout matched (size={}, prefix={})", driverId_.toStdString(),
                    frame.payload.size(), hexPrefix(frame.payload).toStdString());
    }
}

bool SchemaDecoder::layoutMatches(const LayoutMatch& match, const QByteArray& payload) noexcept {
    return codec::layoutMatches(match, payload);
}

bool SchemaDecoder::tryDecodeFrame(const signalforge::frame::RawFrame& frame, std::shared_ptr<SignalValueSink> sink,
                                   std::shared_ptr<const std::vector<signalforge::buffer::SignalBuffer*>> cache) {
    const QByteArray& payload = frame.payload;
    // Fast-path cache (raw pointer view to avoid shared_ptr atomic deref in the
    // inner loop). The shared_ptr keeps the underlying vector alive for the
    // duration of this call.
    signalforge::buffer::SignalBuffer* const* cacheData = cache ? cache->data() : nullptr;
    const std::size_t cacheSize = cache ? cache->size() : 0;
    auto emitSignal = [&](int metaIndex, std::chrono::steady_clock::time_point t, const SignalValue& value,
                          const QString& sigId) -> bool {
        if (cacheData != nullptr && metaIndex >= 0 && static_cast<std::size_t>(metaIndex) < cacheSize) {
            if (auto* buf = cacheData[metaIndex]) {
                buf->push(t, value);
                return true;
            }
        }
        if (sink) {
            try {
                sink->onSignal(t, sigId, value);
                return true;
            } catch (const std::exception& e) {
                SF_LOG_ERROR("SchemaDecoder: sink threw on signal '{}': {}", sigId.toStdString(), e.what());
                return false;
            }
        }
        return true;  // no sink: count as emitted (matches prior behavior)
    };

    for (std::size_t li = 0; li < schema_.layouts.size(); ++li) {
        const Layout& layout = schema_.layouts[li];
        if (!layoutMatches(layout.match, payload)) {
            continue;
        }
        if (payload.size() < layout.minPayloadBytes) {
            if (mMalformed_ != nullptr) {
                mMalformed_->add(1);
            }
            SF_LOG_WARN("SchemaDecoder[{}]: layout '{}' matched but size {} < min_payload_bytes {}",
                        driverId_.toStdString(), layout.name.toStdString(), payload.size(), layout.minPayloadBytes);
            return true;  // matched even though malformed; do not fall through to unmatched.
        }

        const CachedLayout& cached = layoutCache_[li];
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(payload.constData());
        const int payloadSize = payload.size();
        std::int64_t emittedThisFrame = 0;

        for (std::size_t fi = 0; fi < layout.fields.size(); ++fi) {
            const FieldDef& field = layout.fields[fi];
            const CachedField& cf = cached.fields[fi];
            const Endianness end = field.endianness.value_or(layout.endianness);

            if (field.offset + field.sizeBytes > payloadSize) {
                if (mMalformed_ != nullptr) {
                    mMalformed_->add(1);
                }
                SF_LOG_WARN("SchemaDecoder[{}]: field '{}' at offset {} size {} exceeds payload size {}",
                            driverId_.toStdString(), field.name.toStdString(), field.offset, field.sizeBytes,
                            payloadSize);
                return true;
            }

            const std::uint8_t* fieldBytes = bytes + field.offset;

            if (field.encoding == FieldEncoding::BitField) {
                const std::uint64_t raw = readUnsigned(fieldBytes, field.sizeBytes, end);
                for (std::size_t bi = 0; bi < field.bitFields.size(); ++bi) {
                    const BitFieldDef& bit = field.bitFields[bi];
                    const int meta = cf.bitFieldMetaIndices[bi];
                    const QString& sigId = metadataCatalog_[meta].id;
                    const std::uint64_t mask =
                        (bit.bitCount >= 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bit.bitCount) - 1);
                    const std::uint64_t slice = (raw >> bit.bitStart) & mask;
                    SignalValue value;
                    if (bit.bitCount == 1) {
                        value = SignalValue{slice != 0};
                    } else {
                        value = SignalValue{static_cast<std::int64_t>(slice)};
                    }
                    if (emitSignal(meta, frame.recvAt, value, sigId)) {
                        ++emittedThisFrame;
                    }
                }
                continue;
            }

            if (field.encoding == FieldEncoding::FixedString) {
                const QByteArray slice(reinterpret_cast<const char*>(fieldBytes), field.sizeBytes);
                int len = slice.indexOf('\0');
                if (len < 0) {
                    len = field.sizeBytes;
                }
                const QString text = QString::fromUtf8(slice.constData(), len);
                const QString& sigId = metadataCatalog_[cf.metaIndex].id;
                if (emitSignal(cf.metaIndex, frame.recvAt, SignalValue{text}, sigId)) {
                    ++emittedThisFrame;
                }
                continue;
            }

            if (encodingIsFloat(field.encoding)) {
                const double raw = (field.encoding == FieldEncoding::Float32) ? readFloat32(fieldBytes, end)
                                                                              : readFloat64(fieldBytes, end);
                double value = raw;
                if (field.scale.has_value()) {
                    value *= *field.scale;
                }
                if (field.offsetTransform.has_value()) {
                    value += *field.offsetTransform;
                }
                const QString& sigId = metadataCatalog_[cf.metaIndex].id;
                if (emitSignal(cf.metaIndex, frame.recvAt, SignalValue{value}, sigId)) {
                    ++emittedThisFrame;
                }
                continue;
            }

            // Integer encodings.
            const std::uint64_t raw = readUnsigned(fieldBytes, field.sizeBytes, end);
            std::int64_t intRaw;
            if (encodingIsSignedInt(field.encoding)) {
                intRaw = signExtend(raw, field.sizeBytes);
            } else {
                intRaw = static_cast<std::int64_t>(raw);
            }

            const bool transformed = field.scale.has_value() || field.offsetTransform.has_value();
            const QString& sigId = metadataCatalog_[cf.metaIndex].id;
            SignalValue value;
            if (transformed) {
                double v = static_cast<double>(intRaw);
                if (field.scale.has_value()) {
                    v *= *field.scale;
                }
                if (field.offsetTransform.has_value()) {
                    v += *field.offsetTransform;
                }
                value = SignalValue{v};
            } else {
                value = SignalValue{intRaw};
            }
            if (emitSignal(cf.metaIndex, frame.recvAt, value, sigId)) {
                ++emittedThisFrame;
            }
        }

        if (mDecoded_ != nullptr) {
            mDecoded_->add(1);
        }
        if (mEmitted_ != nullptr && emittedThisFrame > 0) {
            mEmitted_->add(emittedThisFrame);
        }
        return true;
    }

    return false;
}

}  // namespace signalforge::decoder

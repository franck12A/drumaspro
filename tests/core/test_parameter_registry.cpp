#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string_view>

#include "core/ParameterRegistry.h"

namespace {

using drumas::core::ParameterCurve;
using drumas::core::ParameterDescriptor;
using drumas::core::ParameterRegistry;
using drumas::core::ParameterUnit;
using drumas::core::makeParameterId;

constexpr auto kGainId = makeParameterId("test.gain");
constexpr auto kCutoffId = makeParameterId("test.cutoff");

[[nodiscard]] ParameterDescriptor gainDescriptor() {
    return ParameterDescriptor{kGainId, "test.gain", 0.0F, 1.0F, 0.25F, ParameterUnit::Percent,
                               ParameterCurve::Linear};
}

[[nodiscard]] ParameterDescriptor cutoffDescriptor() {
    return ParameterDescriptor{kCutoffId, "test.cutoff", 20.0F, 20000.0F, 1000.0F,
                               ParameterUnit::Hertz, ParameterCurve::Logarithmic};
}

}  // namespace

TEST_CASE("parameter ids are stable hashes of their names", "[core][parameters]") {
    REQUIRE(makeParameterId("filter.cutoff") == makeParameterId("filter.cutoff"));
    REQUIRE(makeParameterId("filter.cutoff") != makeParameterId("filter.cutoff2"));
    REQUIRE(makeParameterId("filter.cutoff") != drumas::core::kInvalidParameterId);

    // Pinned value: changing the hash function silently would invalidate every
    // preset and every command id, so the algorithm is part of the contract.
    REQUIRE(makeParameterId("filter.cutoff") == 0xBF58F22EU);
}

TEST_CASE("registry stores metadata and finds by id", "[core][parameters]") {
    ParameterRegistry registry;
    REQUIRE(registry.empty());

    const ParameterDescriptor descriptor = gainDescriptor();
    REQUIRE(registry.add(descriptor));
    REQUIRE(registry.size() == 1);
    REQUIRE_FALSE(registry.empty());

    const ParameterDescriptor* found = registry.find(kGainId);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == std::string_view("test.gain"));
    REQUIRE(found->minValue == 0.0F);
    REQUIRE(found->maxValue == 1.0F);
    REQUIRE(found->defaultValue == Catch::Approx(0.25F));
    REQUIRE(found->unit == ParameterUnit::Percent);
    REQUIRE(found->curve == ParameterCurve::Linear);

    REQUIRE(registry.at(0).id == kGainId);
    REQUIRE(registry.contains(kGainId));
    REQUIRE(registry.find(makeParameterId("test.missing")) == nullptr);
    REQUIRE_FALSE(registry.contains(makeParameterId("test.missing")));
    REQUIRE(registry.defaultValue(kGainId) == Catch::Approx(0.25F));
    REQUIRE(registry.defaultValue(makeParameterId("test.missing")) == 0.0F);
}

TEST_CASE("registry rejects invalid descriptors", "[core][parameters]") {
    ParameterRegistry registry;

    ParameterDescriptor descriptor = gainDescriptor();
    descriptor.id = drumas::core::kInvalidParameterId;
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.name = {};
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.minValue = 1.0F;
    descriptor.maxValue = 1.0F;
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.minValue = 2.0F;
    descriptor.maxValue = 1.0F;
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.maxValue = std::numeric_limits<float>::infinity();
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.defaultValue = 1.5F;
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.defaultValue = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(registry.add(descriptor));

    descriptor = gainDescriptor();
    descriptor.curve = ParameterCurve::Logarithmic;
    descriptor.minValue = 0.0F;
    REQUIRE_FALSE(registry.add(descriptor));

    REQUIRE(registry.empty());
}

TEST_CASE("registry rejects duplicate ids and duplicate names", "[core][parameters]") {
    ParameterRegistry registry;
    REQUIRE(registry.add(gainDescriptor()));

    // Same id, different name.
    ParameterDescriptor duplicateId = cutoffDescriptor();
    duplicateId.id = kGainId;
    duplicateId.name = "test.other";
    REQUIRE_FALSE(registry.add(duplicateId));

    // Different id, same name.
    ParameterDescriptor duplicateName = gainDescriptor();
    duplicateName.id = makeParameterId("test.other");
    REQUIRE_FALSE(registry.add(duplicateName));

    REQUIRE(registry.size() == 1);
}

TEST_CASE("linear parameter maps between real and normalized values", "[core][parameters]") {
    ParameterRegistry registry;
    REQUIRE(registry.add(gainDescriptor()));

    float normalized = -1.0F;
    REQUIRE(registry.toNormalized(kGainId, 0.25F, normalized));
    REQUIRE(normalized == Catch::Approx(0.25F));

    float real = -1.0F;
    REQUIRE(registry.toReal(kGainId, 0.75F, real));
    REQUIRE(real == Catch::Approx(0.75F));

    // Out-of-range real values clamp instead of producing a slider outside its
    // travel; the caller gets a usable value and no error.
    REQUIRE(registry.toNormalized(kGainId, -3.0F, normalized));
    REQUIRE(normalized == Catch::Approx(0.0F));
    REQUIRE(registry.toNormalized(kGainId, 3.0F, normalized));
    REQUIRE(normalized == Catch::Approx(1.0F));

    // Out-of-range normalized values clamp to the real range.
    REQUIRE(registry.toReal(kGainId, -3.0F, real));
    REQUIRE(real == Catch::Approx(0.0F));
    REQUIRE(registry.toReal(kGainId, 3.0F, real));
    REQUIRE(real == Catch::Approx(1.0F));

    // Round trip.
    for (const float value : {0.0F, 0.1F, 0.5F, 0.999F, 1.0F}) {
        REQUIRE(registry.toNormalized(kGainId, value, normalized));
        REQUIRE(registry.toReal(kGainId, normalized, real));
        REQUIRE(real == Catch::Approx(value).margin(1e-6F));
    }
}

TEST_CASE("logarithmic parameter maps equal ratios to equal normalized steps",
          "[core][parameters]") {
    ParameterRegistry registry;
    REQUIRE(registry.add(cutoffDescriptor()));

    // 0.5 of the normalized travel is the geometric mean of the range.
    float real = 0.0F;
    REQUIRE(registry.toReal(kCutoffId, 0.5F, real));
    REQUIRE(real == Catch::Approx(std::sqrt(20.0F * 20000.0F)).epsilon(1e-5));

    // Doubling the frequency is the same distance everywhere in the range.
    float first = 0.0F;
    float second = 0.0F;
    REQUIRE(registry.toNormalized(kCutoffId, 100.0F, first));
    REQUIRE(registry.toNormalized(kCutoffId, 200.0F, second));
    REQUIRE(second - first == Catch::Approx(0.10034F).margin(1e-4F));

    float normalized = 0.0F;
    for (const float frequency : {20.0F, 100.0F, 1000.0F, 10000.0F, 20000.0F}) {
        REQUIRE(registry.toNormalized(kCutoffId, frequency, normalized));
        REQUIRE(registry.toReal(kCutoffId, normalized, real));
        REQUIRE(real == Catch::Approx(frequency).epsilon(1e-4F));
    }

    // Clamping still applies at both ends.
    REQUIRE(registry.toNormalized(kCutoffId, 5.0F, normalized));
    REQUIRE(normalized == Catch::Approx(0.0F));
    REQUIRE(registry.toNormalized(kCutoffId, 50000.0F, normalized));
    REQUIRE(normalized == Catch::Approx(1.0F));
}

TEST_CASE("range clamping reports and rejects what it cannot clamp", "[core][parameters]") {
    ParameterRegistry registry;
    REQUIRE(registry.add(gainDescriptor()));

    float value = 0.5F;
    REQUIRE(registry.clampToRange(kGainId, value));
    REQUIRE(value == Catch::Approx(0.5F));

    value = 2.0F;
    REQUIRE(registry.clampToRange(kGainId, value));
    REQUIRE(value == Catch::Approx(1.0F));

    value = -2.0F;
    REQUIRE(registry.clampToRange(kGainId, value));
    REQUIRE(value == Catch::Approx(0.0F));

    value = std::numeric_limits<float>::infinity();
    REQUIRE(registry.clampToRange(kGainId, value));
    REQUIRE(value == Catch::Approx(1.0F));

    value = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(registry.clampToRange(kGainId, value));

    value = 0.5F;
    REQUIRE_FALSE(registry.clampToRange(makeParameterId("test.missing"), value));
    REQUIRE(value == Catch::Approx(0.5F));
}

TEST_CASE("unknown ids are rejected by every conversion", "[core][parameters]") {
    ParameterRegistry registry;

    float output = 42.0F;
    REQUIRE_FALSE(registry.toNormalized(makeParameterId("test.missing"), 0.5F, output));
    REQUIRE(output == 42.0F);
    REQUIRE_FALSE(registry.toReal(makeParameterId("test.missing"), 0.5F, output));
    REQUIRE(output == 42.0F);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(registry.add(gainDescriptor()));
    REQUIRE_FALSE(registry.toNormalized(kGainId, nan, output));
    REQUIRE_FALSE(registry.toReal(kGainId, nan, output));
}

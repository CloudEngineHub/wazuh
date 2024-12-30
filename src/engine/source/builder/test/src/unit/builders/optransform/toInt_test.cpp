#include "builders/baseBuilders_test.hpp"

#include "builders/opmap/opBuilderHelperMap.hpp"

using namespace builder::builders;

namespace
{
auto customRefExpected()
{
    return [](const BuildersMocks& mocks)
    {
        EXPECT_CALL(*mocks.ctx, validator());
        EXPECT_CALL(*mocks.validator, hasField(DotPath("ref"))).WillOnce(testing::Return(false));
        return None {};
    };
}

auto customRefExpected(json::Json jValue)
{
    return [=](const BuildersMocks& mocks)
    {
        EXPECT_CALL(*mocks.ctx, validator());
        EXPECT_CALL(*mocks.validator, hasField(DotPath("ref"))).WillOnce(testing::Return(false));
        return jValue;
    };
}

auto jTypeRefExpected(json::Json::Type jType)
{
    return [=](const BuildersMocks& mocks)
    {
        EXPECT_CALL(*mocks.ctx, validator());
        EXPECT_CALL(*mocks.ctx, validator()).Times(testing::AtLeast(1));
        EXPECT_CALL(*mocks.validator, hasField(DotPath("ref"))).WillOnce(testing::Return(true));
        EXPECT_CALL(*mocks.validator, getJsonType(DotPath("ref"))).WillOnce(testing::Return(jType));
        return None {};
    };
}

} // namespace

namespace transformbuildtest
{
INSTANTIATE_TEST_SUITE_P(Builders,
                         TransformBuilderTest,
                         testing::Values(
                             /*** to_int ***/
                             TransformT({}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeValue(R"("true")")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref")}, opBuilderHelperToInt, SUCCESS()),
                             TransformT({makeRef("ref"), makeRef("ref")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"(1)")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"(1.1)")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"(true)")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"(null)")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"([1,2,3,4])")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"("c")")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeRef(R"("truncate")")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeRef(R"("round")")}, opBuilderHelperToInt, FAILURE()),
                             TransformT({makeRef("ref"), makeValue(R"("truncate")")}, opBuilderHelperToInt, SUCCESS()),
                             TransformT({makeRef("ref"), makeValue(R"("round")")}, opBuilderHelperToInt, SUCCESS())),
                         testNameFormatter<TransformBuilderTest>("ToInt"));
} // namespace transformbuildtest

namespace transformoperatestest
{
INSTANTIATE_TEST_SUITE_P(
    Builders,
    TransformOperationTest,
    testing::Values(
        /*** to_int ***/
        /*** invalid type target field ***/
        TransformT(R"({"target": "--Strvalue--", "ref": "some"})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("truncate")")},
                   FAILURE()),
        TransformT(R"({"target": 2.2343434, "ref": "some"})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("truncate")")},
                   FAILURE()),
        TransformT(R"({"target": 2.2, "ref": "some"})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("truncate")")},
                   FAILURE()),
        TransformT(R"({"target": [1,2,3], "ref": "some"})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("truncate")")},
                   FAILURE()),
        TransformT(R"({"target": {"key": "value"}, "ref": "some"})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("truncate")")},
                   FAILURE()),
        /*** invalid type reference field ***/
        TransformT(R"({"target": 1, "ref": "some"})", opBuilderHelperToInt, "target", {makeRef("ref")}, FAILURE()),
        TransformT(R"({"target": 1, "ref": 1})", opBuilderHelperToInt, "target", {makeRef("ref")}, FAILURE()),
        TransformT(R"({"target": 1, "ref": "[1,2,3,4]"})", opBuilderHelperToInt, "target", {makeRef("ref")}, FAILURE()),
        TransformT(
            R"({"target": 1, "ref": {"key": "value"}})", opBuilderHelperToInt, "target", {makeRef("ref")}, FAILURE()),
        /*** success cases ***/
        TransformT(R"({"target": 1, "ref": -4.176666736602783})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref")},
                   SUCCESS(makeEvent(R"({"target":-4, "ref": -4.176666736602783})"))),
        TransformT(R"({"target": 1, "ref": -4.176666736602783})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("round")")},
                   SUCCESS(makeEvent(R"({"target":-4, "ref": -4.176666736602783})"))),
        TransformT(R"({"target": 1, "ref": 0.7124601006507874})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("round")")},
                   SUCCESS(makeEvent(R"({"target":1, "ref": 0.7124601006507874})"))),
        TransformT(R"({"target": 1, "ref": 0.7124601006507874})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref")},
                   SUCCESS(makeEvent(R"({"target":0, "ref": 0.7124601006507874})"))),
        TransformT(R"({"target": 1, "ref": 1.50})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref")},
                   SUCCESS(makeEvent(R"({"target":1, "ref": 1.50})"))),
        TransformT(R"({"target": 1, "ref": 1.49999999})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("round")")},
                   SUCCESS(makeEvent(R"({"target":2, "ref": 1.49999999})"))),
        TransformT(R"({"target": 1, "ref": 1.49999999})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref")},
                   SUCCESS(makeEvent(R"({"target":1, "ref": 1.49999999})"))),
        TransformT(R"({"target": 1, "ref": 1.50})",
                   opBuilderHelperToInt,
                   "target",
                   {makeRef("ref"), makeValue(R"("round")")},
                   SUCCESS(makeEvent(R"({"target":2, "ref": 1.50})")))),
    testNameFormatter<TransformOperationTest>("ToInt"));

}

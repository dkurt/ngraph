//*****************************************************************************
// Copyright 2017-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

// clang-format off
#ifdef ${BACKEND_NAME}_FLOAT_TOLERANCE_BITS
#define DEFAULT_FLOAT_TOLERANCE_BITS ${BACKEND_NAME}_FLOAT_TOLERANCE_BITS
#endif
#ifdef ${BACKEND_NAME}_DOUBLE_TOLERANCE_BITS
#define DEFAULT_DOUBLE_TOLERANCE_BITS ${BACKEND_NAME}_DOUBLE_TOLERANCE_BITS
#endif
// clang-format on

#include "gtest/gtest.h"
#include "ngraph/file_util.hpp"
#include "ngraph/frontend/onnx_import/default_opset.hpp"
#include "ngraph/frontend/onnx_import/onnx.hpp"
#include "util/test_case.hpp"
#include "util/test_control.hpp"
#include "util/test_tools.hpp"
#include "util/type_prop.hpp"

using namespace ngraph;
using namespace ngraph::onnx_import;
using namespace ngraph::test;

static std::string s_manifest = "${MANIFEST}";

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, onnx_dynamic_dims_to_ngraph_dynamic_dims)
{
    // the model represents a linear function A * x + B
    // where all 3 operands are model inputs (no initializers)
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/ab_plus_c.prototxt"));

    const auto& graph_inputs = function->get_parameters();
    EXPECT_EQ(graph_inputs.size(), 3);

    // all inputs in the model have a 2D partial shape {?, 2}
    for (const auto& input : graph_inputs)
    {
        const auto& input_ps = input->get_partial_shape();
        EXPECT_TRUE(input_ps.is_dynamic());

        ASSERT_TRUE(input_ps.rank().is_static());
        EXPECT_EQ(static_cast<size_t>(input_ps.rank()), 2);

        EXPECT_TRUE(input_ps[0].is_dynamic());
        ASSERT_TRUE(input_ps[1].is_static());
        EXPECT_EQ(static_cast<size_t>(input_ps[1]), 2);
    }

    const auto& graph_outputs = function->get_results();
    EXPECT_EQ(graph_outputs.size(), 1);

    const auto out = *(graph_outputs.cbegin());
    const auto& out_ps = out->get_output_partial_shape(0);
    ASSERT_TRUE(out_ps.rank().is_static());
    EXPECT_EQ(static_cast<size_t>(out_ps.rank()), 2);

    EXPECT_TRUE(out_ps[0].is_dynamic());
    ASSERT_TRUE(out_ps[1].is_static());
    EXPECT_EQ(static_cast<size_t>(out_ps[1]), 2);
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, ab_plus_c_inference)
{
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/ab_plus_c.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);

    struct ExpectedValuesGenerator
    {
        int64_t i = 1;
        int64_t operator()()
        {
            const auto ret = i * i + i;
            ++i;
            return ret;
        }
    };

    const size_t NUM_BATCHES_TO_TEST = 5;

    for (size_t batch = 1; batch <= NUM_BATCHES_TO_TEST; ++batch)
    {
        const Shape shape{batch, 2};
        const auto elems_in_tensor = shape_size(shape);

        std::vector<int64_t> input_values(elems_in_tensor);
        std::iota(input_values.begin(), input_values.end(), 1);

        test_case.add_input<int64_t>(shape, input_values);
        test_case.add_input<int64_t>(shape, input_values);
        test_case.add_input<int64_t>(shape, input_values);

        std::vector<int64_t> expected_values(elems_in_tensor);
        std::generate(expected_values.begin(), expected_values.end(), ExpectedValuesGenerator{});
        test_case.add_expected_output<int64_t>(shape, expected_values);

        test_case.run();
    }
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, scalar_initializers_shape_check)
{
    // initializers defined witout the "dims" field should produce Constants with an empty Shape
    // initializers with "dims: 0" should be have the same way (Shape{} not Shape{0})
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/scalar_initializers.prototxt"));

    for (auto ng_node : function->get_ordered_ops())
    {
        if (as_type_ptr<default_opset::Constant>(ng_node))
        {
            EXPECT_EQ(ng_node->get_shape(), Shape{});
        }
    }
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, dynamic_rank_input_check)
{
    // the model contains a single Add operation that takes a fully dynamic input and a scalar
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/a_plus_b_dyn_rank.prototxt"));

    const auto& graph_inputs = function->get_parameters();
    ASSERT_EQ(graph_inputs.size(), 2);

    const auto dyn_rank_input = graph_inputs[0];
    const auto scalar_input = graph_inputs[1];

    EXPECT_TRUE(dyn_rank_input->get_partial_shape().rank().is_dynamic());

    ASSERT_TRUE(scalar_input->get_partial_shape().is_static());
    EXPECT_EQ(scalar_input->get_partial_shape().to_shape(), Shape{});

    const auto& graph_outputs = function->get_results();
    EXPECT_EQ(graph_outputs.size(), 1);

    const auto out = *(graph_outputs.cbegin());
    EXPECT_TRUE(out->get_output_partial_shape(0).rank().is_dynamic());
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, dynamic_rank_input_inference)
{
    // the model contains a single Add operation that takes a fully dynamic input and a scalar
    const auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/a_plus_b_dyn_rank.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);

    const size_t RANKS_TO_TEST = 3;
    const int64_t SCALAR_INPUT_VAL = 5;

    for (size_t r = 0; r <= RANKS_TO_TEST; ++r)
    {
        const Shape shape(r, 2);
        const auto elems_in_tensor = shape_size(shape);

        std::vector<int64_t> input_values(elems_in_tensor);
        std::iota(input_values.begin(), input_values.end(), 1);

        test_case.add_input<int64_t>(shape, input_values);
        test_case.add_input<int64_t>(Shape{}, {SCALAR_INPUT_VAL});

        std::vector<int64_t> expected_values(elems_in_tensor);
        std::iota(expected_values.begin(), expected_values.end(), SCALAR_INPUT_VAL + 1);
        test_case.add_expected_output<int64_t>(shape, expected_values);

        test_case.run();
    }
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_acosh_1_3)
{
    auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/acosh_dyn_shape.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);
    test_case.add_input<float>(Shape{1, 3}, {1.0f, 2.5f, 4.3f});
    test_case.add_expected_output<float>(Shape{1, 3}, {0.0f, 1.5667993f, 2.1379586f});

    test_case.run();
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_acosh_3_2)
{
    auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/acosh_dyn_shape.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);
    test_case.add_input<float>(Shape{3, 2}, {1.0f, 2.5f, 4.3f, 1.0f, 2.5f, 4.3f});
    test_case.add_expected_output<float>(
        Shape{3, 2}, {0.0f, 1.5667993f, 2.1379586f, 0.0f, 1.5667993f, 2.1379586f});

    test_case.run();
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_asinh_1_3)
{
    auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/asinh_dyn_shape.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);
    test_case.add_input<float>(Shape{1, 3}, {-1.5f, 0.0f, 1.5f});
    test_case.add_expected_output<float>(Shape{1, 3}, {-1.1947632f, 0.0f, 1.1947632f});

    test_case.run();
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_asinh_3_2)
{
    auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/asinh_dyn_shape.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);
    test_case.add_input<float>(Shape{3, 2}, {-1.5f, 0.0f, 1.5f, -1.5f, 0.0f, 1.5f});
    test_case.add_expected_output<float>(
        Shape{3, 2}, {-1.1947632f, 0.0f, 1.1947632f, -1.1947632, 0.0f, 1.1947632f});

    test_case.run();
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_atanh_1_3)
{
    auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/atanh_dyn_shape.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);
    test_case.add_input<float>(Shape{1, 3}, {-0.9f, 0.0f, 0.9f});
    test_case.add_expected_output<float>(Shape{1, 3}, {-1.47221948f, 0.0f, 1.47221948f});

    test_case.run();
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_atanh_3_2)
{
    auto function = onnx_import::import_onnx_model(
        file_util::path_join(SERIALIZED_ZOO, "onnx/dynamic_shapes/atanh_dyn_shape.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);
    test_case.add_input<float>(Shape{3, 2}, {-0.9f, 0.0f, 0.9f, -0.9f, 0.0f, 0.9f});
    test_case.add_expected_output<float>(
        Shape{3, 2}, {-1.47221948f, 0.0f, 1.47221948f, -1.47221948f, 0.0f, 1.47221948f});

    test_case.run();
}

NGRAPH_TEST(onnx_dyn_shapes_${BACKEND_NAME}, model_conv_with_dynamic_batch)
{
    const auto function = onnx_import::import_onnx_model(file_util::path_join(
        SERIALIZED_ZOO, "onnx/dynamic_shapes/conv_with_dynamic_batch.prototxt"));

    auto test_case = NgraphTestCase(function, "${BACKEND_NAME}", BackendMode::DYNAMIC);

    const auto data_shape = Shape{1, 3, 7, 7};
    const auto filters_shape = Shape{10, 3, 2, 2};
    const auto data_elems = shape_size(data_shape);
    const auto filters_elems = shape_size(filters_shape);

    test_case.add_input<int64_t>(data_shape, std::vector<int64_t>(data_elems, 1));
    test_case.add_input<int64_t>(filters_shape, std::vector<int64_t>(filters_elems, 1));
    test_case.add_input<int64_t>(Shape{10}, std::vector<int64_t>(10, 1));

    const auto expected_out_shape = Shape{1, 10, 6, 6};
    const std::vector<int64_t> expected_values(shape_size(expected_out_shape), 13);
    test_case.add_expected_output<int64_t>(expected_out_shape, expected_values);

    test_case.run();
}

//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
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

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>

#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "util/all_close.hpp"
#include "util/all_close_f.hpp"
#include "util/ndarray.hpp"
#include "util/random.hpp"
#include "util/test_control.hpp"
#include "util/test_tools.hpp"

static std::mt19937_64 random_generator;

using namespace std;
using namespace ngraph;

static string s_manifest = "${MANIFEST}";

NGRAPH_TEST(${BACKEND_NAME}, cum_sum_default)
{
    Shape shape{1, 4};
    auto A = make_shared<op::Parameter>(element::f32, shape);
    auto f = make_shared<Function>(make_shared<op::CumSum>(A, 1), ParameterVector{A});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    // Create some tensors for input/output
    auto a = backend->create_tensor(element::f32, shape);
    copy_data(a, vector<float>{1, 2, 3, 4});
    auto result = backend->create_tensor(element::f32, shape);

    auto handle = backend->compile(f);
    handle->call_with_validate({result}, {a});
    EXPECT_TRUE(test::all_close_f((vector<float>{1, 3, 6, 10}), read_vector<float>(result)));
}

NGRAPH_TEST(${BACKEND_NAME}, cum_sum_2dim)
{
    Shape shape{2, 4};
    auto A = make_shared<op::Parameter>(element::f32, shape);
    auto f = make_shared<Function>(make_shared<op::CumSum>(A, 0), ParameterVector{A});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    // Create some tensors for input/output
    auto a = backend->create_tensor(element::f32, shape);
    copy_data(a, vector<float>{0, 1, 2, 3, 4, 5, 6, 7});
    auto result = backend->create_tensor(element::f32, shape);

    auto handle = backend->compile(f);
    handle->call_with_validate({result}, {a});
    EXPECT_TRUE(
        test::all_close_f((vector<float>{0, 1, 2, 3, 4, 6, 8, 10}), read_vector<float>(result)));
}

NGRAPH_TEST(${BACKEND_NAME}, cum_sum_3d)
{
    auto test_cumsum_3d = [](const int64_t axis) -> void {
        Shape shape{3, 2, 4};
        auto A = make_shared<op::Parameter>(element::f32, shape);
        auto f = make_shared<Function>(make_shared<op::CumSum>(A, axis), ParameterVector{A});

        auto backend = runtime::Backend::create("${BACKEND_NAME}");

        // Create some tensors for input/output
        auto a = backend->create_tensor(element::f32, shape);
        copy_data(a, vector<float>{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23});
        auto result = backend->create_tensor(element::f32, shape);

        auto handle = backend->compile(f);
        handle->call_with_validate({result}, {a});

        if (axis == 0)
        {
            EXPECT_TRUE(
                test::all_close_f((vector<float>{0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 12, 14,
                                                 16, 18, 20, 22, 24, 27, 30, 33, 36, 39, 42, 45}),
                                  read_vector<float>(result)));
        }
        else if (axis == 1)
        {
            EXPECT_TRUE(
                test::all_close_f((vector<float>{0,  1,  2,  3,  4,  6,  8,  10, 8,  9,  10, 11,
                                                 20, 22, 24, 26, 16, 17, 18, 19, 36, 38, 40, 42}),
                                  read_vector<float>(result)));
        }
        else if (axis == 2)
        {
            EXPECT_TRUE(
                test::all_close_f((vector<float>{0,  1,  3,  6,  4,  9,  15, 22, 8,  17, 27, 38,
                                                 12, 25, 39, 54, 16, 33, 51, 70, 20, 41, 63, 86}),
                                  read_vector<float>(result)));
        }
    };
    test_cumsum_3d(0);
    test_cumsum_3d(1);
    test_cumsum_3d(2);
}
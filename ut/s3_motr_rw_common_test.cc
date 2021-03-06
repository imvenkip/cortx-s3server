/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

#include <event2/event.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_evhtp_wrapper.h"
#include "mock_s3_asyncop_context_base.h"
#include "mock_s3_motr_wrapper.h"
#include "mock_s3_request_object.h"
#include "s3_callback_test_helpers.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::AtLeast;

class S3MotrReadWriteCommonTest : public testing::Test {
 protected:
  S3MotrReadWriteCommonTest() {
    evbase_t *evbase = event_base_new();
    evhtp_request_t *req = evhtp_request_new(NULL, evbase);
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, new EvhtpWrapper());
    ptr_mock_s3motr = std::make_shared<MockS3Motr>();
    EXPECT_CALL(*ptr_mock_s3motr, motr_op_rc(_)).WillRepeatedly(Return(0));
    ptr_mock_s3_async_context = std::make_shared<MockS3AsyncOpContextBase>(
        ptr_mock_request,
        std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
        std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj),
        ptr_mock_s3motr);
    s3user_event =
        event_new(evbase, -1, EV_WRITE | EV_READ | EV_TIMEOUT, NULL, NULL);
    user_context = (struct user_event_context *)calloc(
        1, sizeof(struct user_event_context));
    user_context->app_ctx =
        static_cast<void *>(ptr_mock_s3_async_context.get());
    user_context->user_event = (void *)s3user_event;
  }

  std::shared_ptr<MockS3Motr> ptr_mock_s3motr;
  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3AsyncOpContextBase> ptr_mock_s3_async_context;
  S3CallBack s3objectmetadata_callbackobj;
  struct event *s3user_event;
  struct user_event_context *user_context;
};

TEST_F(S3MotrReadWriteCommonTest, MotrOpDoneOnMainThreadOnSuccess) {
  ptr_mock_s3_async_context->at_least_one_success = true;
  motr_op_done_on_main_thread(1, 1, (void *)user_context);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3MotrReadWriteCommonTest, MotrOpDoneOnMainThreadOnFail) {
  motr_op_done_on_main_thread(1, 1, (void *)user_context);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3MotrReadWriteCommonTest, S3MotrOpStableResponseCountSameAsOpCount) {
  struct m0_op op;
  struct s3_motr_context_obj *op_ctx = (struct s3_motr_context_obj *)calloc(
      1, sizeof(struct s3_motr_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_s3_async_context,
              set_op_status_for(_, S3AsyncOpStatus::success, "Success."))
      .Times(1);
  EXPECT_CALL(*ptr_mock_s3motr, motr_op_rc(_)).WillOnce(Return(0));
  ptr_mock_s3_async_context->response_received_count = 0;
  ptr_mock_s3_async_context->ops_count = 1;
  s3_motr_op_stable(&op);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3MotrReadWriteCommonTest, S3MotrOpStableResponseCountNotSameAsOpCount) {
  struct m0_op op;
  struct s3_motr_context_obj *op_ctx = (struct s3_motr_context_obj *)calloc(
      1, sizeof(struct s3_motr_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_s3_async_context,
              set_op_status_for(_, S3AsyncOpStatus::success, "Success."))
      .Times(1);
  EXPECT_CALL(*ptr_mock_s3motr, motr_op_rc(_))
      .WillOnce(Return(0))
      .WillRepeatedly(Return(-EPERM));
  ptr_mock_s3_async_context->response_received_count = 1;
  ptr_mock_s3_async_context->ops_count = 3;
  s3_motr_op_stable(&op);
  EXPECT_FALSE(s3objectmetadata_callbackobj.success_called);
  EXPECT_FALSE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3MotrReadWriteCommonTest, S3MotrOpFailedResponseCountSameAsOpCount) {
  struct m0_op op;
  struct s3_motr_context_obj *op_ctx = (struct s3_motr_context_obj *)calloc(
      1, sizeof(struct s3_motr_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_s3_async_context,
              set_op_status_for(_, S3AsyncOpStatus::failed,
                                "Operation Failed.")).Times(1);
  ptr_mock_s3_async_context->response_received_count = 0;
  ptr_mock_s3_async_context->ops_count = 1;
  s3_motr_op_failed(&op);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3MotrReadWriteCommonTest, S3MotrOpFailedResponseCountNotSameAsOpCount) {
  struct m0_op op;
  struct s3_motr_context_obj *op_ctx = (struct s3_motr_context_obj *)calloc(
      1, sizeof(struct s3_motr_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_s3_async_context,
              set_op_status_for(_, S3AsyncOpStatus::failed,
                                "Operation Failed.")).Times(1);
  ptr_mock_s3_async_context->response_received_count = 0;
  ptr_mock_s3_async_context->ops_count = 3;
  s3_motr_op_failed(&op);
  EXPECT_FALSE(s3objectmetadata_callbackobj.success_called);
  EXPECT_FALSE(s3objectmetadata_callbackobj.fail_called);
}

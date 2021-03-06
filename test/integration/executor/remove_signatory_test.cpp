/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "integration/executor/executor_fixture.hpp"

#include <gtest/gtest.h>
#include "cryptography/public_key.hpp"
#include "framework/common_constants.hpp"
#include "framework/result_gtest_checkers.hpp"
#include "integration/executor/command_permission_test.hpp"
#include "integration/executor/executor_fixture_param_provider.hpp"
#include "module/shared_model/mock_objects_factories/mock_command_factory.hpp"
#include "module/shared_model/mock_objects_factories/mock_query_factory.hpp"

using namespace common_constants;
using namespace executor_testing;
using namespace framework::expected;

using shared_model::interface::permissions::Grantable;
using shared_model::interface::permissions::Role;

static const shared_model::crypto::PublicKey kTargetSignatory{
    "target_signatory"};

class RemoveSignatoryTest : public ExecutorTestBase {
 public:
  void addTargetUser(const shared_model::interface::RolePermissionSet &perms) {
    assertResultValue(getItf().createUserWithPerms(
        kUser, kDomain, kUserKeypair.publicKey(), perms));
  }

  void addSignatory() {
    assertResultValue(getItf().executeMaintenanceCommand(
        *getItf().getMockCommandFactory()->constructAddSignatory(
            kTargetSignatory, kUserId)));
  }

  auto issueRemoveSignatoryBy(
      const shared_model::interface::types::AccountIdType &issuer,
      bool validation_enabled = true) {
    return getItf().executeCommandAsAccount(
        *getItf().getMockCommandFactory()->constructRemoveSignatory(
            kUserId, kTargetSignatory),
        issuer,
        validation_enabled);
  }

 protected:
  const shared_model::crypto::PublicKey &old_sig_{kUserKeypair.publicKey()};
};

using RemoveSignatoryBasicTest = BasicExecutorTest<RemoveSignatoryTest>;

/**
 * @given a user with RemoveSignatory permission
 * @when execute RemoveSignatory command with nonexistent target user
 * @then the command fails
 */
TEST_P(RemoveSignatoryBasicTest, NonExistentUser) {
  assertResultValue(
      getItf().createUserWithPerms(kSecondUser,
                                   kDomain,
                                   kSameDomainUserKeypair.publicKey(),
                                   {Role::kRemoveSignatory}));

  checkCommandError(issueRemoveSignatoryBy(kSameDomainUserId), 3);
}

/**
 * @given some user with RemoveSignatory permission and 1 signatory
 * @when user executes RemoveSignatory for his own account and 2nd signatory
 * @then the command fails and his signatories are unchanged
 */
TEST_P(RemoveSignatoryBasicTest, NoSuchSignatory) {
  ASSERT_NO_FATAL_FAILURE(addTargetUser({Role::kRemoveSignatory}));
  ASSERT_NO_FATAL_FAILURE(checkSignatories(kUserId, {old_sig_}););

  checkCommandError(issueRemoveSignatoryBy(kUserId), 4);

  checkSignatories(kUserId, {old_sig_});
}

/**
 * @given some user with RemoveSignatory permission, 2 signatories and quorum 2
 * @when user executes RemoveSignatory for his own account and 2nd signatory
 * @then the command fails and his signatories are unchanged
 */
TEST_P(RemoveSignatoryBasicTest, SignatoriesLessThanQuorum) {
  ASSERT_NO_FATAL_FAILURE(addTargetUser({Role::kRemoveSignatory}));
  ASSERT_NO_FATAL_FAILURE(addSignatory());
  assertResultValue(getItf().executeMaintenanceCommand(
      *getItf().getMockCommandFactory()->constructSetQuorum(kUserId, 2)));
  ASSERT_NO_FATAL_FAILURE(
      checkSignatories(kUserId, {old_sig_, kTargetSignatory}););

  checkCommandError(issueRemoveSignatoryBy(kUserId), 5);

  checkSignatories(kUserId, {old_sig_, kTargetSignatory});
}

INSTANTIATE_TEST_CASE_P(Base,
                        RemoveSignatoryBasicTest,
                        executor_testing::getExecutorTestParams(),
                        executor_testing::paramToString);

using RemoveSignatoryPermissionTest =
    command_permission_test::CommandPermissionTest<RemoveSignatoryTest>;

TEST_P(RemoveSignatoryPermissionTest, CommandPermissionTest) {
  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kSecondDomain));
  ASSERT_NO_FATAL_FAILURE(prepareState({}));
  ASSERT_NO_FATAL_FAILURE(addSignatory());
  ASSERT_NO_FATAL_FAILURE(
      checkSignatories(kUserId, {old_sig_, kTargetSignatory}));

  if (checkResponse(
          issueRemoveSignatoryBy(getActor(), getValidationEnabled()))) {
    checkSignatories(kUserId, {old_sig_});
  } else {
    checkSignatories(kUserId, {old_sig_, kTargetSignatory});
  }
}

INSTANTIATE_TEST_CASE_P(
    Common,
    RemoveSignatoryPermissionTest,
    command_permission_test::getParams(Role::kRemoveSignatory,
                                       boost::none,
                                       boost::none,
                                       Grantable::kRemoveMySignatory),
    command_permission_test::paramToString);

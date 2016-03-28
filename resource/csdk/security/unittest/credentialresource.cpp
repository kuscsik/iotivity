//******************************************************************
//
// Copyright 2015 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "gtest/gtest.h"
#include "logger.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "resourcemanager.h"
#include "credresource.h"
#include "securevirtualresourcetypes.h"
#include "srmtestcommon.h"
#include "srmutility.h"
#include "psinterface.h"
#include "security_internals.h"

#define TAG "SRM-CRED-UT"

OicSecCred_t * getCredList()
{
    size_t sz = 0;
    OicSecCred_t *cred = (OicSecCred_t *)OICCalloc(1, sizeof(*cred));
    VERIFY_NON_NULL(TAG, cred, ERROR);
    cred->credId = 1234;
    OICStrcpy((char *)cred->subject.id, sizeof(cred->subject.id)+1, "1111111111111111");

#if 0
    cred->roleIdsLen = 2;
    cred->roleIds = (OicSecRole_t *)OICCalloc(cred->roleIdsLen, sizeof(OicSecRole_t));
    VERIFY_NON_NULL(TAG, cred->roleIds, ERROR);
    OICStrcpy((char *)cred->roleIds[0].id, sizeof(cred->roleIds[0].id), "role11");
    OICStrcpy((char *)cred->roleIds[1].id, sizeof(cred->roleIds[1].id), "role12");

#endif

    cred->credType = SYMMETRIC_PAIR_WISE_KEY;
    cred->privateData.data = (uint8_t *)OICCalloc(1, strlen("My private Key11") + 1);
    VERIFY_NON_NULL(TAG, cred->privateData.data, ERROR);
    OICStrcpy((char *)cred->privateData.data, strlen("My private Key11")+1,"My private Key11");
    OICStrcpy((char *)cred->rownerID.id, sizeof(cred->rownerID.id), "aaaaaaaaaaaaaaaa");
    cred->next = (OicSecCred_t*)OICCalloc(1, sizeof(*cred->next));
    VERIFY_NON_NULL(TAG, cred->next, ERROR);
    cred->next->credId = 5678;
    OICStrcpy((char *)cred->next->subject.id, sizeof(cred->next->subject.id)+1, "2222222222222222");
#if 0
    cred->next->roleIdsLen = 0;
#endif
    cred->next->credType = SYMMETRIC_PAIR_WISE_KEY;
    sz = strlen("My private Key21") + 1;
    cred->next->privateData.data = (uint8_t *)OICCalloc(1, sz);
    VERIFY_NON_NULL(TAG, cred->next->privateData.data, ERROR);
    OICStrcpy((char *)cred->next->privateData.data, sz, "My private Key21");
#if 0
    sz = strlen("My Public Key123") + 1
    cred->next->publicData.data = (char *)OICCalloc(1, sz);
    VERIFY_NON_NULL(TAG, cred->next->publicData.data, ERROR);
    OICStrcpy(cred->next->publicData.data, sz,"My Public Key123");
#endif
    OICStrcpy((char *)cred->next->rownerID.id, sizeof(cred->next->rownerID.id), "bbbbbbbbbbbbbbbb");

    return cred;

exit:
    if(cred)
    {
        DeleteCredList(cred);
        cred = NULL;
    }
    return cred;
}

static void printCred(const OicSecCred_t * cred)
{
    EXPECT_TRUE(NULL != cred);

    const OicSecCred_t *credTmp1 = NULL;
    for (credTmp1 = cred; credTmp1; credTmp1 = credTmp1->next)
    {
        OIC_LOG_V(INFO, TAG, "\ncred->credId = %d", credTmp1->credId);
        OIC_LOG_V(INFO, TAG, "cred->subject.id = %s", credTmp1->subject.id);
        OIC_LOG_V(INFO, TAG, "cred->credType = %d", credTmp1->credType);
        if(credTmp1->privateData.data)
        {
            OIC_LOG_V(INFO, TAG, "cred->privateData.data = %s", credTmp1->privateData.data);
        }
#ifdef __WITH_X509__
        if(credTmp1->publicData.data)
        {
           OIC_LOG_V(INFO, TAG, "cred->publicData.data = %s", credTmp1->publicData.data);
        }
#endif /* __WITH_X509__ */
	OIC_LOG_V(INFO, TAG, "cred->rownerID = %s", credTmp1->rownerID.id);
    }
}

 //InitCredResource Tests
TEST(CredResourceTest, InitCredResource)
{
    EXPECT_EQ(OC_STACK_INVALID_PARAM, InitCredResource());
}

//DeInitCredResource Tests
TEST(CredResourceTest, DeInitCredResource)
{
    EXPECT_EQ(OC_STACK_INVALID_PARAM, DeInitCredResource());
}

//CreateCredResource Tests
TEST(CredResourceTest, CreateCredResource)
{
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CreateCredResource());
}

 //CredEntityHandler Tests
TEST(CredResourceTest, CredEntityHandlerWithDummyRequest)
{
    OCEntityHandlerRequest req = OCEntityHandlerRequest();
    EXPECT_EQ(OC_EH_ERROR,
            CredEntityHandler(OCEntityHandlerFlag::OC_REQUEST_FLAG, &req, NULL));
}

TEST(CredResourceTest, CredEntityHandlerWithNULLRequest)
{
    EXPECT_EQ(OC_EH_ERROR,
            CredEntityHandler(OCEntityHandlerFlag::OC_REQUEST_FLAG, NULL, NULL));
}

TEST(CredResourceTest, CredEntityHandlerInvalidFlag)
{
    OCEntityHandlerRequest req = OCEntityHandlerRequest();
    EXPECT_EQ(OC_EH_ERROR,
            CredEntityHandler(OCEntityHandlerFlag::OC_OBSERVE_FLAG, &req, NULL));
}

//Cred DELETE request
TEST(CredResourceTest, CredEntityHandlerDeleteTest)
{
    OCEntityHandlerRequest ehReq =  OCEntityHandlerRequest();
    static OCPersistentStorage ps =  OCPersistentStorage();
    const OicSecCred_t* subjectCred1 = NULL;
    const OicSecCred_t* subjectCred2 = NULL;
    OCEntityHandlerResult ehRet = OC_EH_ERROR;
    char query[] = "subjectuuid=31313131-3131-3131-3131-313131313131"; //canonical uuid of subject1

    SetPersistentHandler(&ps, true);

    OicSecCred_t *cred = getCredList();
    ASSERT_TRUE(NULL != cred);
    uint8_t *payload = NULL;
    size_t size = 0;
    EXPECT_EQ(OC_STACK_OK, CredToCBORPayload(cred, &payload, &size));
    if (!payload)
    {
        DeleteCredList(cred);
    }
    ASSERT_TRUE(NULL != payload);

    // Create Entity Handler POST request payload
    ehReq.method = OC_REST_POST;
    ehReq.payload = (OCPayload *)OCSecurityPayloadCreate(payload, size);
    if (!ehReq.payload)
    {
        OICFree(payload);
        DeleteCredList(cred);
    }
    ASSERT_TRUE( NULL != ehReq.payload);
    EXPECT_EQ(OC_EH_ERROR, CredEntityHandler(OC_REQUEST_FLAG, &ehReq, NULL));

    // Verify if SRM contains Credential for the subject
    subjectCred1 = GetCredResourceData(&cred->subject);
    EXPECT_TRUE(NULL != subjectCred1);

    // Create Entity Handler DELETE request
    ehReq.method = OC_REST_DELETE;
    ehReq.query = (char *)OICCalloc(1, strlen(query)+1);
    if (!ehReq.query)
    {
        OICFree(payload);
        DeleteCredList(cred);
    }
    ASSERT_TRUE(NULL != ehReq.query);
    OICStrcpy(ehReq.query, strlen(query)+1, query);

    ehRet = CredEntityHandler(OC_REQUEST_FLAG, &ehReq, NULL);
    EXPECT_EQ(OC_EH_ERROR, ehRet);

    // Verify if SRM has deleted ACE for the subject
    subjectCred2 = GetCredResourceData(&cred->subject);
    EXPECT_TRUE(NULL == subjectCred2);

    // Perform cleanup
    OICFree(ehReq.query);
    OICFree(payload);
    DeInitCredResource();
    DeleteCredList(cred);
    OCPayloadDestroy((OCPayload *)ehReq.payload);
}

TEST(CredResourceTest, CredToCBORPayloadNULL)
{
    OicSecCred_t *cred = getCredList();
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CredToCBORPayload(NULL, NULL, 0));
    size_t size = 0;
    uint8_t *cborPayload = (uint8_t *) OICCalloc(1, 10);
    if (!cborPayload)
    {
        DeleteCredList(cred);
    }
    ASSERT_TRUE(NULL != cborPayload);
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CredToCBORPayload(cred, &cborPayload, &size));
    OICFree(cborPayload);
    cborPayload = NULL;
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CredToCBORPayload(NULL, &cborPayload, &size));
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CredToCBORPayload(cred, &cborPayload, 0));
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CredToCBORPayload(cred, NULL, &size));
    DeleteCredList(cred);
}

TEST(CredResourceTest, CredToCBORPayloadVALID)
{
    uint8_t* payload = NULL;
    size_t size = 0;
    OicSecCred_t *cred = getCredList();

    EXPECT_EQ(OC_STACK_OK, CredToCBORPayload(cred, &payload, &size));
    if (!payload)
    {
        DeleteCredList(cred);
    }
    ASSERT_TRUE(NULL != payload);

    DeleteCredList(cred);
    OICFree(payload);
}

TEST(CredResourceTest, CBORPayloadToCredVALID)
{
    OicSecCred_t *cred1 = getCredList();

    uint8_t *payload = NULL;
    size_t size = 0;
    EXPECT_EQ(OC_STACK_OK, CredToCBORPayload(cred1, &payload, &size));
    DeleteCredList(cred1);
    ASSERT_TRUE(NULL != payload);

    OicSecCred_t *cred2 = NULL;
    EXPECT_EQ(OC_STACK_OK, CBORPayloadToCred(payload, size, &cred2));
    OICFree(payload);
    ASSERT_TRUE(cred2 != NULL);
    DeleteCredList(cred2);
}

TEST(CredResourceTest, CBORPayloadToCredNULL)
{
    OicSecCred_t *cred = NULL;
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CBORPayloadToCred(NULL, 0, NULL));
    uint8_t *cborPayload = (uint8_t *) OICCalloc(1, 10);
    ASSERT_TRUE(NULL != cborPayload);
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CBORPayloadToCred(NULL, 0, &cred));
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CBORPayloadToCred(cborPayload, 0, NULL));
    cred = getCredList();
    EXPECT_EQ(OC_STACK_INVALID_PARAM, CBORPayloadToCred(cborPayload, 0, &cred));
    DeleteCredList(cred);
    OICFree(cborPayload);
}

//GetCredResourceData Test
TEST(CredResourceTest, GetCredResourceDataNULLSubject)
{
    EXPECT_EQ(NULL, GetCredResourceData(NULL));
}

TEST(CredResourceTest, GenerateCredentialValidInput)
{
    OicUuid_t rownerID = {{0}};
    OICStrcpy((char *)rownerID.id, strlen("ownersId21"), "ownersId21");

    OicUuid_t subject = {{0}};
    OICStrcpy((char *)subject.id, strlen("subject11"), "subject11");

    uint8_t privateKey[] = "My private Key11";
    OicSecKey_t key = {privateKey, sizeof(privateKey)};

    OicSecCred_t * cred  = NULL;
    cred = GenerateCredential(&subject, SYMMETRIC_PAIR_WISE_KEY, NULL,
                              &key, &rownerID);
    printCred(cred);

    ASSERT_TRUE(NULL != cred);
    DeleteCredList(cred);
}

TEST(CredResourceTest, GenerateAndAddCredentialValidInput)
{
    OicUuid_t rownerID = {{0}};
    OICStrcpy((char *)rownerID.id, sizeof(rownerID.id), "ownersId11");

    OicUuid_t subject = {{0}};
    OICStrcpy((char *)subject.id, sizeof(subject.id), "subject11");

    uint8_t privateKey[] = "My private Key11";
    OicSecKey_t key = {privateKey, sizeof(privateKey)};

    OicSecCred_t *cred1  = NULL;
    OicSecCred_t *headCred = NULL;

    cred1 = GenerateCredential(&subject, SYMMETRIC_PAIR_WISE_KEY, NULL,
                               &key, &rownerID);

    EXPECT_EQ(OC_STACK_OK, AddCredential(cred1));
    headCred = cred1;

    OICStrcpy((char *)rownerID.id, sizeof(rownerID.id), "ownersId22");
    OICStrcpy((char *)subject.id, sizeof(subject.id), "subject22");
    cred1 = GenerateCredential(&subject, SYMMETRIC_PAIR_WISE_KEY, NULL,
                               &key, &rownerID);
    EXPECT_EQ(OC_STACK_OK, AddCredential(cred1));

    OICStrcpy((char *)rownerID.id, sizeof(rownerID.id), "ownersId33");
    OICStrcpy((char *)subject.id, sizeof(subject.id), "subject33");
    cred1 = GenerateCredential(&subject, SYMMETRIC_PAIR_WISE_KEY, NULL,
                               &key, &rownerID);
    EXPECT_EQ(OC_STACK_OK, AddCredential(cred1));

    const OicSecCred_t* credList = GetCredResourceData(&headCred->subject);

    printCred(credList);

    DeleteCredList(headCred);
}

#if 0
TEST(CredGetResourceDataTest, GetCredResourceDataValidSubject)
{
    OicSecCred_t* cred = getCredList();
    EXPECT_TRUE(NULL != GetCredResourceData(cred->subject));
}
#endif

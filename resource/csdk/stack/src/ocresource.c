//******************************************************************
//
// Copyright 2014 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Defining _POSIX_C_SOURCE macro with 200112L (or greater) as value
// causes header files to expose definitions
// corresponding to the POSIX.1-2001 base
// specification (excluding the XSI extension).
// For POSIX.1-2001 base specification,
// Refer http://pubs.opengroup.org/onlinepubs/009695399/
#define _POSIX_C_SOURCE 200112L
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "iotivity_config.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "ocresource.h"
#include "ocresourcehandler.h"
#include "ocobserve.h"
#include "occollection.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "logger.h"
#include "ocpayload.h"
#include "secureresourcemanager.h"
#include "cacommon.h"
#include "cainterface.h"
#include "ocpayload.h"
#include "oickeepalive.h"
#include "platform_features.h"
#include "payload_logging.h"
#include "ocendpoint.h"
#include "ocstackinternal.h"
#include "oickeepalive.h"

#ifdef ROUTING_GATEWAY
#include "routingmanager.h"
#endif

/// Module Name
#define TAG "OIC_RI_RESOURCE"

// using 1k as block size since most persistent storage implementations use a power of 2.
#define INTROSPECTION_FILE_SIZE_BLOCK 1024

#define VERIFY_SUCCESS(op) { if (op != (OC_STACK_OK)) \
            {OIC_LOG_V(FATAL, TAG, "%s failed!!", #op); goto exit;} }

extern OCResource *headResource;

/**
 * Prepares a Payload for response.
 */
static OCStackResult BuildVirtualResourceResponse(const OCResource *resourcePtr,
                                                  OCDiscoveryPayload *payload,
                                                  OCDevAddr *endpoint,
                                                  CAEndpoint_t *networkInfo,
                                                  size_t infoSize);

//-----------------------------------------------------------------------------
// Default resource entity handler function
//-----------------------------------------------------------------------------
OCEntityHandlerResult defaultResourceEHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest * request, void* callbackParam)
{
    //TODO ("Implement me!!!!");
    // TODO:  remove silence unused param warnings
    (void) flag;
    (void) request;
    (void) callbackParam;
    return  OC_EH_OK; // Making sure that the Default EH and the Vendor EH have matching signatures
}

/* This method will retrieve the port at which the secure resource is hosted */
static OCStackResult GetSecurePortInfo(OCDevAddr *endpoint, uint16_t *port)
{
    uint16_t p = 0;

    if (endpoint->adapter == OC_ADAPTER_IP)
    {
        if (endpoint->flags & OC_IP_USE_V6)
        {
            p = caglobals.ip.u6s.port;
        }
        else if (endpoint->flags & OC_IP_USE_V4)
        {
            p = caglobals.ip.u4s.port;
        }
    }

    *port = p;
    return OC_STACK_OK;
}

#ifdef TCP_ADAPTER
/* This method will retrieve the tcp port */
static OCStackResult GetTCPPortInfo(OCDevAddr *endpoint, uint16_t *port, bool secured)
{
    uint16_t p = 0;

    if (endpoint->adapter == OC_ADAPTER_IP)
    {
        if (endpoint->flags & OC_IP_USE_V4)
        {
            p = secured ? caglobals.tcp.ipv4s.port : caglobals.tcp.ipv4.port;
        }
        else if (endpoint->flags & OC_IP_USE_V6)
        {
            p = secured ? caglobals.tcp.ipv6s.port : caglobals.tcp.ipv6.port;
        }
    }

    *port = p;
    return OC_STACK_OK;
}
#endif

/*
 * Function will extract 0, 1 or 2 filters from query.
 * More than 2 filters or unsupported filters will result in error.
 * If both filters are of the same supported type, the 2nd one will be picked.
 * Resource and device filters in the SAME query are NOT validated
 * and resources will likely not clear filters.
 */
OCStackResult ExtractFiltersFromQuery(const char *query, char **filterOne, char **filterTwo)
{
    if (!query)
    {
        OIC_LOG(ERROR, TAG, "Query is empty!");
        return OC_STACK_INVALID_QUERY;
    }
    char *key = NULL;
    char *value = NULL;
    char *queryDup = NULL;
    char *restOfQuery = NULL;
    char *keyValuePair = NULL;
    int numKeyValuePairsParsed = 0;

    *filterOne = NULL;
    *filterTwo = NULL;

    queryDup = OICStrdup(query);
    if (NULL == queryDup)
    {
        OIC_LOG(ERROR, TAG, "Creating duplicate string failed!");
        return OC_STACK_NO_MEMORY;
    }

    OIC_LOG_V(INFO, TAG, "Extracting params from %s", queryDup);

    OCStackResult eCode = OC_STACK_INVALID_QUERY;
    if (strnlen(queryDup, MAX_QUERY_LENGTH) >= MAX_QUERY_LENGTH)
    {
        OIC_LOG(ERROR, TAG, "Query exceeds maximum length.");
        goto exit;
    }

    keyValuePair = strtok_r (queryDup, OC_QUERY_SEPARATOR, &restOfQuery);

    while(keyValuePair)
    {
        if (numKeyValuePairsParsed >= 2)
        {
            OIC_LOG(ERROR, TAG, "More than 2 queries params in URI.");
            goto exit;
        }

        key = strtok_r(keyValuePair, OC_KEY_VALUE_DELIMITER, &value);

        if (!key || !value)
        {
            goto exit;
        }
        else if (strncasecmp(key, OC_RSRVD_INTERFACE, sizeof(OC_RSRVD_INTERFACE) - 1) == 0)
        {
            *filterOne = value;     // if
        }
        else if (strncasecmp(key, OC_RSRVD_RESOURCE_TYPE, sizeof(OC_RSRVD_INTERFACE) - 1) == 0)
        {
            *filterTwo = value;     // rt
        }
        else
        {
            OIC_LOG_V(ERROR, TAG, "Unsupported query key: %s", key);
            goto exit;
        }
        ++numKeyValuePairsParsed;

        keyValuePair = strtok_r(NULL, OC_QUERY_SEPARATOR, &restOfQuery);
    }

    if (*filterOne)
    {
        *filterOne = OICStrdup(*filterOne);
        if (NULL == *filterOne)
        {
            OIC_LOG(ERROR, TAG, "Creating duplicate string failed!");
            eCode = OC_STACK_NO_MEMORY;
            goto exit;
        }
    }

    if (*filterTwo)
    {
        *filterTwo = OICStrdup(*filterTwo);
        if (NULL == *filterTwo)
        {
            OIC_LOG(ERROR, TAG, "Creating duplicate string failed!");
            OICFree(*filterOne);
            eCode = OC_STACK_NO_MEMORY;
            goto exit;
        }
    }

    OICFree(queryDup);
    OIC_LOG_V(INFO, TAG, "Extracted params if: %s and rt: %s.", *filterOne, *filterTwo);
    return OC_STACK_OK;

exit:
    *filterOne = NULL;
    *filterTwo = NULL;
    OICFree(queryDup);
    return eCode;
}

OCVirtualResources GetTypeOfVirtualURI(const char *uriInRequest)
{
    if (strcmp(uriInRequest, OC_RSRVD_WELL_KNOWN_URI) == 0)
    {
        return OC_WELL_KNOWN_URI;
    }
    else if (strcmp(uriInRequest, OC_RSRVD_DEVICE_URI) == 0)
    {
        return OC_DEVICE_URI;
    }
    else if (strcmp(uriInRequest, OC_RSRVD_PLATFORM_URI) == 0)
    {
        return OC_PLATFORM_URI;
    }
    else if (strcmp(uriInRequest, OC_RSRVD_RESOURCE_TYPES_URI) == 0)
    {
        return OC_RESOURCE_TYPES_URI;
    }
    else if (strcmp(uriInRequest, OC_RSRVD_INTROSPECTION_URI) == 0)
    {
        return OC_INTROSPECTION_URI;
    }
    else if (strcmp(uriInRequest, OC_RSRVD_INTROSPECTION_PAYLOAD_URI) == 0)
    {
        return OC_INTROSPECTION_PAYLOAD_URI;
    }
#ifdef ROUTING_GATEWAY
    else if (0 == strcmp(uriInRequest, OC_RSRVD_GATEWAY_URI))
    {
        return OC_GATEWAY_URI;
    }
#endif
#ifdef WITH_PRESENCE
    else if (strcmp(uriInRequest, OC_RSRVD_PRESENCE_URI) == 0)
    {
        return OC_PRESENCE;
    }
#endif //WITH_PRESENCE

#ifdef MQ_BROKER
    else if (0 == strcmp(uriInRequest, OC_RSRVD_WELL_KNOWN_MQ_URI))
    {
        return OC_MQ_BROKER_URI;
    }
#endif //MQ_BROKER

#ifdef TCP_ADAPTER
    else if (strcmp(uriInRequest, OC_RSRVD_KEEPALIVE_URI) == 0)
    {
        return OC_KEEPALIVE_RESOURCE_URI;
    }
#endif

    return OC_UNKNOWN_URI;
}

static OCStackResult getQueryParamsForFiltering (OCVirtualResources uri, char *query,
                                            char **filterOne, char **filterTwo)
{
    if(!filterOne || !filterTwo)
    {
        return OC_STACK_INVALID_PARAM;
    }

    *filterOne = NULL;
    *filterTwo = NULL;

#ifdef WITH_PRESENCE
    if (uri == OC_PRESENCE)
    {
        //Nothing needs to be done, except for pass a OC_PRESENCE query through as OC_STACK_OK.
        OIC_LOG(INFO, TAG, "OC_PRESENCE Request for virtual resource.");
        return OC_STACK_OK;
    }
#endif

    OCStackResult result = OC_STACK_OK;

    if (query && *query)
    {
        result = ExtractFiltersFromQuery(query, filterOne, filterTwo);
    }

    return result;
}

static OCStackResult BuildDevicePlatformPayload(const OCResource *resourcePtr, OCRepPayload** payload,
    bool addDeviceId)
{
    OCRepPayload *tempPayload = OCRepPayloadCreate();

    if (!resourcePtr)
    {
        OCRepPayloadDestroy(tempPayload);
        return OC_STACK_INVALID_PARAM;
    }

    if (!tempPayload)
    {
        return OC_STACK_NO_MEMORY;
    }

    if (addDeviceId)
    {
        const char *deviceId = OCGetServerInstanceIDString();
        if (!deviceId)
        {
            OIC_LOG(ERROR, TAG, "Failed retrieving device id.");
            return OC_STACK_ERROR;
        }
        OCRepPayloadSetPropString(tempPayload, OC_RSRVD_DEVICE_ID, deviceId);
    }

    for (OCResourceType *resType = resourcePtr->rsrcType; resType; resType = resType->next)
    {
        OCRepPayloadAddResourceType(tempPayload, resType->resourcetypename);
    }

    for (OCResourceInterface *resInterface = resourcePtr->rsrcInterface; resInterface;
        resInterface = resInterface->next)
    {
        OCRepPayloadAddInterface(tempPayload, resInterface->name);
    }

    for (OCAttribute *resAttrib = resourcePtr->rsrcAttributes; resAttrib; resAttrib = resAttrib->next)
    {
        if (resAttrib->attrName && resAttrib->attrValue)
        {
            if (0 == strcmp(OC_RSRVD_DATA_MODEL_VERSION, resAttrib->attrName))
            {
                char *dmv = OCCreateString((OCStringLL *)resAttrib->attrValue);
                if (dmv)
                {
                    OCRepPayloadSetPropString(tempPayload, resAttrib->attrName, dmv);
                    OICFree(dmv);
                }
            }
            else
            {
                OCRepPayloadSetPropString(tempPayload, resAttrib->attrName, (char *)resAttrib->attrValue);
            }
        }
    }

    if (!*payload)
    {
        *payload = tempPayload;
    }
    else
    {
        OCRepPayloadAppend(*payload, tempPayload);
    }

    return OC_STACK_OK;
}

OCStackResult BuildResponseRepresentation(const OCResource *resourcePtr,
                    OCRepPayload** payload, OCDevAddr *devAddr)
{
    OCRepPayload *tempPayload = OCRepPayloadCreate();

    if (!resourcePtr)
    {
        OCRepPayloadDestroy(tempPayload);
        return OC_STACK_INVALID_PARAM;
    }

    if(!tempPayload)
    {
        return OC_STACK_NO_MEMORY;
    }

    OCRepPayloadSetPropString(tempPayload, OC_RSRVD_HREF, resourcePtr->uri);

    uint8_t numElement = 0;
    if (OC_STACK_OK == OCGetNumberOfResourceTypes((OCResource *)resourcePtr, &numElement))
    {
        size_t rtDim[MAX_REP_ARRAY_DEPTH] = {numElement, 0, 0};
        char **rt = (char **)OICMalloc(sizeof(char *) * numElement);
        for (uint8_t i = 0; i < numElement; ++i)
        {
            const char *value = OCGetResourceTypeName((OCResource *)resourcePtr, i);
            OIC_LOG_V(DEBUG, TAG, "value: %s", value);
            rt[i] = OICStrdup(value);
        }
        OCRepPayloadSetStringArrayAsOwner(tempPayload, OC_RSRVD_RESOURCE_TYPE, rt, rtDim);
    }

    numElement = 0;
    if (OC_STACK_OK == OCGetNumberOfResourceInterfaces((OCResource *)resourcePtr, &numElement))
    {
        size_t ifDim[MAX_REP_ARRAY_DEPTH] = {numElement, 0, 0};
        char **itf = (char **)OICMalloc(sizeof(char *) * numElement);
        for (uint8_t i = 0; i < numElement; ++i)
        {
            const char *value = OCGetResourceInterfaceName((OCResource *)resourcePtr, i);
            OIC_LOG_V(DEBUG, TAG, "value: %s", value);
            itf[i] = OICStrdup(value);
        }
        OCRepPayloadSetStringArrayAsOwner(tempPayload, OC_RSRVD_INTERFACE, itf, ifDim);
    }

    for (OCAttribute *resAttrib = resourcePtr->rsrcAttributes; resAttrib; resAttrib = resAttrib->next)
    {
        if (resAttrib->attrName && resAttrib->attrValue)
        {
            OCRepPayloadSetPropString(tempPayload, resAttrib->attrName, (char *)resAttrib->attrValue);
        }
    }

    OCResourceProperty p = OCGetResourceProperties((OCResourceHandle *)resourcePtr);
    OCRepPayload *policy = OCRepPayloadCreate();
    if (!policy)
    {
        OCPayloadDestroy((OCPayload *)tempPayload);
        return OC_STACK_NO_MEMORY;
    }
    OCRepPayloadSetPropInt(policy, OC_RSRVD_BITMAP, ((p & OC_DISCOVERABLE) | (p & OC_OBSERVABLE)));
    if (p & OC_SECURE)
    {
        OCRepPayloadSetPropBool(policy, OC_RSRVD_SECURE, p & OC_SECURE);
        uint16_t securePort = 0;
        if (GetSecurePortInfo(devAddr, &securePort) != OC_STACK_OK)
        {
            securePort = 0;
        }
        OCRepPayloadSetPropInt(policy, OC_RSRVD_HOSTING_PORT, securePort);
    }
    OCRepPayloadSetPropObjectAsOwner(tempPayload, OC_RSRVD_POLICY, policy);

    if (!*payload)
    {
        *payload = tempPayload;
    }
    else
    {
        OCRepPayloadAppend(*payload, tempPayload);
    }

    return OC_STACK_OK;
}

static size_t GetIntrospectionDataSize(const OCPersistentStorage *ps)
{
    size_t size = 0;
    char buffer[INTROSPECTION_FILE_SIZE_BLOCK];
    FILE *fp;

    if (!ps)
    {
        return 0;
    }

    fp = ps->open(OC_INTROSPECTION_FILE_NAME, "rb");
    if (fp)
    {
        size_t bytesRead = 0;
        do
        {
            bytesRead = ps->read(buffer, 1, INTROSPECTION_FILE_SIZE_BLOCK, fp);
            size += bytesRead;
        } while (bytesRead);
        ps->close(fp);
    }
    return size;
}

OCStackResult GetIntrospectionDataFromPS(char **data, size_t *size)
{
    OIC_LOG(DEBUG, TAG, "GetIntrospectionDataFromPS IN");

    FILE *fp = NULL;
    uint8_t *fsData = NULL;
    size_t fileSize = 0;
    OCStackResult ret = OC_STACK_ERROR;
    OCPersistentStorage *ps = NULL;

    if (!data || *data || !size)
    {
        return OC_STACK_INVALID_PARAM;
    }

    ps = OCGetPersistentStorageHandler();
    if (!ps)
    {
        OIC_LOG(ERROR, TAG, "Persistent Storage handler is NULL");
        goto exit;
    }

    fileSize = GetIntrospectionDataSize(ps);
    OIC_LOG_V(DEBUG, TAG, "File Read Size: %zu", fileSize);
    if (fileSize)
    {
        // allocate one more byte to accomodate null terminator for string we are reading.
        fsData = (uint8_t *)OICCalloc(1, fileSize + 1);
        if (!fsData)
        {
            OIC_LOG(ERROR, TAG, "Could not allocate memory for introspection data");
            goto exit;
        }

        fp = ps->open(OC_INTROSPECTION_FILE_NAME, "rb");
        if (!fp)
        {
            OIC_LOG(ERROR, TAG, "Could not open persistent storage file for introspection data");
            goto exit;
        }
        if (ps->read(fsData, 1, fileSize, fp) == fileSize)
        {
            *size = fileSize;
            fsData[fileSize] = '\0';
            *data = (char *)fsData;
            fsData = NULL;
            ret = OC_STACK_OK;
        }
    }
    OIC_LOG(DEBUG, TAG, "GetIntrospectionDataFromPS OUT");

exit:
    if (fp)
    {
        ps->close(fp);
    }
    if (fsData)
    {
        OICFree(fsData);
    }
    return ret;
}

OCStackResult BuildIntrospectionPayloadResponse(const OCResource *resourcePtr,
    OCRepPayload** payload, OCDevAddr *devAddr)
{
    OCRepPayload *tempPayload = NULL;
    OCStackResult ret;
    char *introspectionData = NULL;
    size_t size = 0;
    ret = GetIntrospectionDataFromPS(&introspectionData, &size);
    if (OC_STACK_OK == ret)
    {
        OCRepPayload *tempPayload = OCRepPayloadCreate();
        if (tempPayload)
        {
            if (OCRepPayloadSetPropStringAsOwner(tempPayload, OC_RSRVD_INTROSPECTION_DATA_NAME, introspectionData))
            {
                *payload = tempPayload;
            }
        }
        else
        {
            ret = OC_STACK_NO_MEMORY;
        }
    }
    if (ret != OC_STACK_OK)
    {
        OICFree(introspectionData);
        OCRepPayloadDestroy(tempPayload);
    }

    return ret;
}

OCRepPayload *BuildUrlInfoWithProtocol(const char *protocol)
{
    OCStackResult result = OC_STACK_OK;
    OCRepPayload *urlInfoPayload = OCRepPayloadCreate();
    if (!urlInfoPayload)
    {
        OIC_LOG(ERROR, TAG, "Failed to create a new RepPayload");
        result = OC_STACK_NO_MEMORY;
        goto exit;
    }

    if (!OCRepPayloadSetPropString(urlInfoPayload, OC_RSRVD_INTROSPECTION_URL, OC_RSRVD_INTROSPECTION_PAYLOAD_URI))
    {
        OIC_LOG(ERROR, TAG, "Failed to add url");
        result = OC_STACK_ERROR;
        goto exit;
    }
    if (!OCRepPayloadSetPropString(urlInfoPayload, OC_RSRVD_INTROSPECTION_PROTOCOL, protocol))
    {
        OIC_LOG(ERROR, TAG, "Failed to add protocol");
        result = OC_STACK_ERROR;
        goto exit;
    }
    if (!OCRepPayloadSetPropString(urlInfoPayload, OC_RSRVD_INTROSPECTION_CONTENT_TYPE, OC_RSRVD_INTROSPECTION_CONTENT_TYPE_VALUE))
    {
        OIC_LOG(ERROR, TAG, "Failed to add content type");
        result = OC_STACK_ERROR;
        goto exit;
    }
    if (!OCRepPayloadSetPropInt(urlInfoPayload, OC_RSRVD_INTROSPECTION_VERSION, OC_RSRVD_INTROSPECTION_VERSION_VALUE))
    {
        OIC_LOG(ERROR, TAG, "Failed to add version");
        result = OC_STACK_ERROR;
        goto exit;
    }

exit:
    if (result != OC_STACK_OK)
    {
        OCRepPayloadDestroy(urlInfoPayload);
        urlInfoPayload = NULL;
    }
    return urlInfoPayload;
}

OCStackResult AddProtocolToLL(OCStringLL **protoLL, const char *protocol)
{
    OCStringLL* cur = *protoLL;
    // Check if protocol is already in list
    while (cur)
    {
        if (strcmp(cur->value, protocol) == 0)
        {
            break;
        }
        cur = cur->next;
    }
    if (cur)
    {
        // The intent of the protocol list is to collect all unique protocols available on this
        // endpoint. Set an error that can be used to skip processing this protocol further as
        // it already exists in the list.
        return OC_STACK_INVALID_PARAM;
    }
    else
    {
        cur = (OCStringLL*)OICCalloc(1, sizeof(OCStringLL));
        if (!cur)
        {
            return OC_STACK_NO_MEMORY;
        }
    }

    cur->value = OICStrdup(protocol);
    if (!cur->value)
    {
        OICFree(cur);
        return OC_STACK_NO_MEMORY;
    }

    cur->next = *protoLL;
    *protoLL = cur;
    return OC_STACK_OK;
}

void FreeProtocolLL(OCStringLL *protoLL)
{
    OCStringLL* cur = protoLL;
    while (cur)
    {
        OCStringLL *temp = cur;
        cur = cur->next;
        OICFree(temp->value);
        OICFree(temp);
    }
}

OCStackResult BuildIntrospectionResponseRepresentation(const OCResource *resourcePtr,
    OCRepPayload** payload, OCDevAddr *devAddr)
{
    size_t dimensions[3] = { 0, 0, 0 };
    OCRepPayload *tempPayload = NULL;
    OCRepPayload **urlInfoPayload = NULL;
    OCStringLL *protoLL = NULL;
    OCStackResult ret = OC_STACK_OK;
    OCResourceType *resType = NULL;
    OCResourceInterface *resInterface = NULL;

    if (!resourcePtr)
    {
        ret = OC_STACK_INVALID_PARAM;
        goto exit;
    }

    tempPayload = OCRepPayloadCreate();
    if (!tempPayload)
    {
        ret = OC_STACK_NO_MEMORY;
        goto exit;
    }

    if (!OCRepPayloadSetUri(tempPayload, resourcePtr->uri))
    {
        OIC_LOG(ERROR, TAG, "Failed to set payload URI");
        ret = OC_STACK_ERROR;
        goto exit;
    }

    resType = resourcePtr->rsrcType;
    while (resType)
    {
        if (!OCRepPayloadAddResourceType(tempPayload, resType->resourcetypename))
        {
            OIC_LOG(ERROR, TAG, "Failed at add resource type");
            ret = OC_STACK_ERROR;
            goto exit;
        }
        resType = resType->next;
    }

    resInterface = resourcePtr->rsrcInterface;
    while (resInterface)
    {
        if (!OCRepPayloadAddInterface(tempPayload, resInterface->name))
        {
            OIC_LOG(ERROR, TAG, "Failed to add interface");
            ret = OC_STACK_ERROR;
            goto exit;
        }
        resInterface = resInterface->next;
    }
    if (!OCRepPayloadSetPropString(tempPayload, OC_RSRVD_INTROSPECTION_NAME, OC_RSRVD_INTROSPECTION_NAME_VALUE))
    {
        OIC_LOG(ERROR, TAG, "Failed to set Name property.");
        ret = OC_STACK_ERROR;
        goto exit;
    }

    // Figure out which protocols this endpoint supports
    if (resourcePtr->endpointType & OC_COAP)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, COAP_STR))
        {
            dimensions[0]++;
        }
    }
    if (resourcePtr->endpointType & OC_COAPS)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, COAPS_STR))
        {
            dimensions[0]++;
        }
    }
#ifdef TCP_ADAPTER
    if (resourcePtr->endpointType & OC_COAP_TCP)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, COAP_STR))
        {
            dimensions[0]++;
        }
    }
    if (resourcePtr->endpointType & OC_COAPS_TCP)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, COAPS_STR))
        {
            dimensions[0]++;
        }
    }
#endif
#ifdef HTTP_ADAPTER
    if (resourcePtr->endpointType & OC_HTTP)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, HTTP_STR))
        {
            dimensions[0]++;
        }
    }
    if (resourcePtr->endpointType & OC_HTTPS)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, HTTPS_STR))
        {
            dimensions[0]++;
        }
    }
#endif
#ifdef EDR_ADAPTER
    if (resourcePtr->endpointType & OC_COAP_RFCOMM)
    {
        if (OC_STACK_OK == AddProtocolToLL(&protoLL, COAP_STR))
        {
            dimensions[0]++;
        }
    }
#endif
    // Add a urlInfo object for each protocol supported
    if (dimensions[0] >= 0)
    {
        urlInfoPayload = (OCRepPayload **)OICMalloc(dimensions[0] * sizeof(OCRepPayload));
        if (urlInfoPayload)
        {
            OCStringLL *proto = protoLL;
            size_t i = 0;
            while (proto)
            {
                urlInfoPayload[i] = BuildUrlInfoWithProtocol(proto->value);
                if (!urlInfoPayload[i])
                {
                    OIC_LOG(ERROR, TAG, "Unable to build urlInfo object for protocol");
                    ret = OC_STACK_ERROR;
                    goto exit;
                }
                proto = proto->next;
                i++;
            }
            if (!OCRepPayloadSetPropObjectArrayAsOwner(tempPayload,
                                                       OC_RSRVD_INTROSPECTION_URL_INFO,
                                                       urlInfoPayload,
                                                       dimensions))
            {
                OIC_LOG(ERROR, TAG, "Unable to add urlInfo object to introspection payload ");
                ret = OC_STACK_ERROR;
                goto exit;
            }
        }
        else
        {
            OIC_LOG(ERROR, TAG, "Unable to allocate memory for urlInfo ");
            ret = OC_STACK_NO_MEMORY;
            goto exit;
        }
    }

    if (!*payload)
    {
        *payload = tempPayload;
    }
    else
    {
        OCRepPayloadAppend(*payload, tempPayload);
    }
exit:
    if (ret != OC_STACK_OK)
    {
        OCRepPayloadDestroy(tempPayload);
        if (urlInfoPayload)
        {
            OICFree(urlInfoPayload);
        }
    }
    FreeProtocolLL(protoLL);

    return OC_STACK_OK;
}

OCStackResult BuildVirtualResourceResponse(const OCResource *resourcePtr,
                                           OCDiscoveryPayload *payload,
                                           OCDevAddr *devAddr,
                                           CAEndpoint_t *networkInfo,
                                           size_t infoSize)
{
    if (!resourcePtr || !payload)
    {
        return OC_STACK_INVALID_PARAM;
    }
    uint16_t securePort = 0;
    if (resourcePtr->resourceProperties & OC_SECURE)
    {
       if (GetSecurePortInfo(devAddr, &securePort) != OC_STACK_OK)
       {
           securePort = 0;
       }
    }

    bool isVirtual = false;
    if (GetTypeOfVirtualURI(resourcePtr->uri) != OC_UNKNOWN_URI)
    {
        isVirtual = true;
    }
#ifdef TCP_ADAPTER
    uint16_t tcpPort = 0;
    GetTCPPortInfo(devAddr, &tcpPort, (resourcePtr->resourceProperties & OC_SECURE));

    OCDiscoveryPayloadAddResourceWithEps(payload, resourcePtr, securePort,
                                         isVirtual, networkInfo, infoSize, devAddr, tcpPort);
#else
    OCDiscoveryPayloadAddResourceWithEps(payload, resourcePtr, securePort,
                                         isVirtual, networkInfo, infoSize, devAddr);
#endif

    return OC_STACK_OK;
}

OCResource *FindResourceByUri(const char* resourceUri)
{
    if(!resourceUri)
    {
        return NULL;
    }

    OCResource * pointer = headResource;
    while (pointer)
    {
        if (strcmp(resourceUri, pointer->uri) == 0)
        {
            return pointer;
        }
        pointer = pointer->next;
    }
    OIC_LOG_V(INFO, TAG, "Resource %s not found", resourceUri);
    return NULL;
}

OCStackResult CheckRequestsEndpoint(const OCDevAddr *reqDevAddr,
                                    OCTpsSchemeFlags resTpsFlags)
{
    if (!reqDevAddr)
    {
        OIC_LOG(ERROR, TAG, "OCDevAddr* is NULL!!!");
        return OC_STACK_INVALID_PARAM;
    }

    OCTpsSchemeFlags reqTpsFlags = OC_NO_TPS;
    OCStackResult result = OCGetMatchedTpsFlags((CATransportAdapter_t)reqDevAddr->adapter,
                                  (CATransportFlags_t)reqDevAddr->flags, &reqTpsFlags);

    if (result != OC_STACK_OK)
    {
        OIC_LOG_V(ERROR, TAG, "Failed at get TPS flags. errcode is %d", result);
        return result;
    }

    // bit compare between request tps flags and resource tps flags
    if (reqTpsFlags & resTpsFlags)
    {
        OIC_LOG(INFO, TAG, "Request come from registered TPS");
        return OC_STACK_OK;
    }
    else
    {
        OIC_LOG(ERROR, TAG, "Request come from unregistered TPS!!!");
        return OC_STACK_BAD_ENDPOINT;
    }
}

OCStackResult DetermineResourceHandling (const OCServerRequest *request,
                                         ResourceHandling *handling,
                                         OCResource **resource)
{
    if(!request || !handling || !resource)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OIC_LOG_V(INFO, TAG, "DetermineResourceHandling for %s", request->resourceUrl);

    // Check if virtual resource
    if (GetTypeOfVirtualURI(request->resourceUrl) != OC_UNKNOWN_URI)
    {
        OIC_LOG_V (INFO, TAG, "%s is virtual", request->resourceUrl);
        *handling = OC_RESOURCE_VIRTUAL;
        *resource = headResource;
        return OC_STACK_OK;
    }
    if (strlen((const char*)(request->resourceUrl)) == 0)
    {
        // Resource URL not specified
        *handling = OC_RESOURCE_NOT_SPECIFIED;
        return OC_STACK_NO_RESOURCE;
    }
    else
    {
        OCResource *resourcePtr = FindResourceByUri((const char*)request->resourceUrl);
        *resource = resourcePtr;

        // Checking resource TPS flags if resource exist in stack.
        if (resourcePtr)
        {
            OCStackResult result = CheckRequestsEndpoint(&(request->devAddr), resourcePtr->endpointType);

            if (result != OC_STACK_OK)
            {
                if (result == OC_STACK_BAD_ENDPOINT)
                {
                    OIC_LOG(ERROR, TAG, "Request come from bad endpoint. ignore request!!!");
                    return OC_STACK_BAD_ENDPOINT;
                }
                else
                {
                    OIC_LOG_V(ERROR, TAG, "Failed at get tps flag errcode: %d", result);
                    return result;
                }
            }
        }

        if (!resourcePtr)
        {
            if(defaultDeviceHandler)
            {
                *handling = OC_RESOURCE_DEFAULT_DEVICE_ENTITYHANDLER;
                return OC_STACK_OK;
            }

            // Resource does not exist
            // and default device handler does not exist
            *handling = OC_RESOURCE_NOT_SPECIFIED;
            return OC_STACK_NO_RESOURCE;
        }

        if (resourcePtr && resourcePtr->rsrcChildResourcesHead != NULL)
        {
            // Collection resource
            if (resourcePtr->entityHandler != defaultResourceEHandler)
            {
                *handling = OC_RESOURCE_COLLECTION_WITH_ENTITYHANDLER;
                return OC_STACK_OK;
            }
            else
            {
                *handling = OC_RESOURCE_COLLECTION_DEFAULT_ENTITYHANDLER;
                return OC_STACK_OK;
            }
        }
        else
        {
            // Resource not a collection
            if (resourcePtr->entityHandler != defaultResourceEHandler)
            {
                *handling = OC_RESOURCE_NOT_COLLECTION_WITH_ENTITYHANDLER;
                return OC_STACK_OK;
            }
            else
            {
                *handling = OC_RESOURCE_NOT_COLLECTION_DEFAULT_ENTITYHANDLER;
                return OC_STACK_OK;
            }
        }
    }
}

OCStackResult EntityHandlerCodeToOCStackCode(OCEntityHandlerResult ehResult)
{
    OCStackResult result;

    switch (ehResult)
    {
        case OC_EH_OK:
        case OC_EH_CONTENT:
        case OC_EH_VALID:
            result = OC_STACK_OK;
            break;
        case OC_EH_SLOW:
            result = OC_STACK_SLOW_RESOURCE;
            break;
        case OC_EH_ERROR:
            result = OC_STACK_ERROR;
            break;
        case OC_EH_FORBIDDEN:
            result = OC_STACK_FORBIDDEN_REQ;
            break;
        case OC_EH_INTERNAL_SERVER_ERROR:
            result = OC_STACK_INTERNAL_SERVER_ERROR;
            break;
        case OC_EH_RESOURCE_CREATED:
            result = OC_STACK_RESOURCE_CREATED;
            break;
        case OC_EH_RESOURCE_DELETED:
            result = OC_STACK_RESOURCE_DELETED;
            break;
        case OC_EH_CHANGED:
            result = OC_STACK_RESOURCE_CHANGED;
            break;
        case OC_EH_RESOURCE_NOT_FOUND:
            result = OC_STACK_NO_RESOURCE;
            break;
        default:
            result = OC_STACK_ERROR;
    }

    return result;
}

static bool resourceMatchesRTFilter(OCResource *resource, char *resourceTypeFilter)
{
    if (!resource)
    {
        return false;
    }

    // Null is analogous to no filter.i.e. query is of form /oic/res?if=oic.if.baseline or /oic/res,
    // without rt query.
    if (NULL == resourceTypeFilter)
    {
        return true;
    }

    // Empty resourceType filter is analogous to error query
    // It is an error as query is of form /oic/res?rt=
    if (0 == strlen(resourceTypeFilter))
    {
        return false;
    }

    for (OCResourceType *rtPtr = resource->rsrcType; rtPtr; rtPtr = rtPtr->next)
    {
        if (0 == strcmp(rtPtr->resourcetypename, resourceTypeFilter))
        {
            return true;
        }
    }

    OIC_LOG_V(INFO, TAG, "%s does not contain rt=%s.", resource->uri, resourceTypeFilter);
    return false;
}

static bool resourceMatchesIFFilter(OCResource *resource, char *interfaceFilter)
{
    if (!resource)
    {
        return false;
    }

    // Null is analogous to no filter i.e. query is of form /oic/res?rt=core.light or /oic/res,
    // without if query.
    if (NULL == interfaceFilter)
    {
        return true;
    }

    // Empty interface filter is analogous to error query
    // It is an error as query is of form /oic/res?if=
    if (0 == strlen(interfaceFilter))
    {
        return false;
    }

    for (OCResourceInterface *ifPtr = resource->rsrcInterface; ifPtr; ifPtr = ifPtr->next)
    {
        if (0 == strcmp(ifPtr->name, interfaceFilter) ||
            0 == strcmp(OC_RSRVD_INTERFACE_LL, interfaceFilter) ||
            0 == strcmp(OC_RSRVD_INTERFACE_DEFAULT, interfaceFilter))
        {
            return true;
        }
    }

    OIC_LOG_V(INFO, TAG, "%s does not contain if=%s.", resource->uri, interfaceFilter);
    return false;
}

/*
 * If the filters are null, they will be assumed to NOT be present
 * and the resource will not be matched against them.
 * Function will return true if all non null AND non empty filters passed in find a match.
 */
static bool includeThisResourceInResponse(OCResource *resource,
                                          char *interfaceFilter,
                                          char *resourceTypeFilter)
{
    if (!resource)
    {
        OIC_LOG(ERROR, TAG, "Invalid resource");
        return false;
    }

    if (resource->resourceProperties & OC_EXPLICIT_DISCOVERABLE)
    {
        /*
         * At least one valid filter should be available to
         * include the resource in discovery response
         */
        if (!(resourceTypeFilter && *resourceTypeFilter))
        {
            OIC_LOG_V(INFO, TAG, "%s no query string for EXPLICIT_DISCOVERABLE\
                resource", resource->uri);
            return false;
        }
    }
    else if (!(resource->resourceProperties & OC_ACTIVE) ||
         !(resource->resourceProperties & OC_DISCOVERABLE))
    {
        OIC_LOG_V(INFO, TAG, "%s not ACTIVE or DISCOVERABLE", resource->uri);
        return false;
    }

    return resourceMatchesIFFilter(resource, interfaceFilter) &&
           resourceMatchesRTFilter(resource, resourceTypeFilter);
}

OCStackResult SendNonPersistantDiscoveryResponse(OCServerRequest *request, OCResource *resource,
                                OCPayload *discoveryPayload, OCEntityHandlerResult ehResult)
{
    OCEntityHandlerResponse *response = NULL;
    OCStackResult result = OC_STACK_ERROR;

    response = (OCEntityHandlerResponse *)OICCalloc(1, sizeof(*response));
    VERIFY_PARAM_NON_NULL(TAG, response, "Failed allocating OCEntityHandlerResponse");

    response->ehResult = ehResult;
    response->payload = discoveryPayload;
    response->persistentBufferFlag = 0;
    response->requestHandle = (OCRequestHandle) request;
    response->resourceHandle = (OCResourceHandle) resource;

    result = OCDoResponse(response);

    OICFree(response);
    return result;

exit:
    return OC_STACK_NO_MEMORY;
}

static OCStackResult EHRequest(OCEntityHandlerRequest *ehRequest, OCPayloadType type,
    OCServerRequest *request, OCResource *resource)
{
    return FormOCEntityHandlerRequest(ehRequest,
                                     (OCRequestHandle)request,
                                     request->method,
                                     &request->devAddr,
                                     (OCResourceHandle)resource,
                                     request->query,
                                     type,
                                     request->payload,
                                     request->payloadSize,
                                     request->numRcvdVendorSpecificHeaderOptions,
                                     request->rcvdVendorSpecificHeaderOptions,
                                     (OCObserveAction)(request->notificationFlag ? OC_OBSERVE_NO_OPTION :
                                                       request->observationOption),
                                     (OCObservationId)0,
                                     request->coapID);
}

#ifdef RD_SERVER
/**
 * Find resources at the resource directory server. These resources are not local resources but
 * remote resources.
 *
 * @param interfaceQuery The interface query parameter.
 * @param resourceTypeQuery The resourceType query parameter.
 * @param discPayload The payload that will be added with the resource information if found at RD.
 *
 * @return ::OC_STACK_OK if any resources are found else ::OC_STACK_NO_RESOURCE.
 * In case if RD server is not started, it returns ::OC_STACK_NO_RESOURCE.
 */
static OCStackResult findResourcesAtRD(const char *interfaceQuery,
                                       const char *resourceTypeQuery, OCDiscoveryPayload **discPayload)
{
    OCStackResult result = OC_STACK_NO_RESOURCE;
    if (OCGetResourceHandleAtUri(OC_RSRVD_RD_URI) != NULL)
    {
        result = OCRDDatabaseDiscoveryPayloadCreate(interfaceQuery, resourceTypeQuery,
            (*discPayload) ? &(*discPayload)->next : discPayload);
    }
    if ((*discPayload) && (*discPayload)->resources)
    {
        result = OC_STACK_OK;
    }
    return result;
}
#endif

/**
 * Creates a discovery payload and add device id information. This information is included in all
 * /oic/res response.
 *
 * @param payload  payload that will have memory alllocated and device id information added.
 *
 * @return ::OC_STACK_OK if successful in allocating memory and adding ID information.
 * ::OC_STACK_NO_MEMORY if failed allocating the memory.
 */
static OCStackResult discoveryPayloadCreateAndAddDeviceId(OCPayload **payload)
{
    if (*payload)
    {
        OIC_LOG(DEBUG, TAG, "Payload is already allocated");
        return OC_STACK_OK;
    }

    *payload = (OCPayload *) OCDiscoveryPayloadCreate();
    VERIFY_PARAM_NON_NULL(TAG, *payload, "Failed adding device id to discovery payload.");

    {
        OCDiscoveryPayload *discPayload = (OCDiscoveryPayload *)*payload;
        discPayload->sid = (char *)OICCalloc(1, UUID_STRING_SIZE);
        VERIFY_PARAM_NON_NULL(TAG, discPayload->sid, "Failed adding device id to discovery payload.");

        const char* uid = OCGetServerInstanceIDString();
        if (uid)
        {
            memcpy(discPayload->sid, uid, UUID_STRING_SIZE);
        }

    }
    return OC_STACK_OK;
exit:
    OCPayloadDestroy(*payload);
    return OC_STACK_NO_MEMORY;
}

/**
 * Add the common properties to the payload, they are only included in case of oic.if.baseline response.
 *
 * @param discPayload payload that will have the baseline information included.
 *
 * @return ::OC_STACK_OK if successful in adding all the information. ::OC_STACK_NO_MEMORY if failed
 * allocating the memory for the baseline information.
 */
static OCStackResult addDiscoveryBaselineCommonProperties(OCDiscoveryPayload *discPayload)
{
    if (!discPayload)
    {
        OIC_LOG(ERROR, TAG, "Payload is not allocated");
        return OC_STACK_INVALID_PARAM;
    }

    OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, (void **)&discPayload->name);

    discPayload->type = (OCStringLL*)OICCalloc(1, sizeof(OCStringLL));
    VERIFY_PARAM_NON_NULL(TAG, discPayload->type, "Failed adding rt to discovery payload.");
    discPayload->type->value = OICStrdup(OC_RSRVD_RESOURCE_TYPE_RES);
    VERIFY_PARAM_NON_NULL(TAG, discPayload->type, "Failed adding rt value to discovery payload.");

    OCResourcePayloadAddStringLL(&discPayload->iface, OC_RSRVD_INTERFACE_LL);
    OCResourcePayloadAddStringLL(&discPayload->iface, OC_RSRVD_INTERFACE_DEFAULT);
    VERIFY_PARAM_NON_NULL(TAG, discPayload->iface, "Failed adding if to discovery payload.");

    return OC_STACK_OK;

exit:
    return OC_STACK_NO_MEMORY;
}

static bool isUnicast(OCServerRequest *request)
{
    bool isMulticast = request->devAddr.flags & OC_MULTICAST;
    return (isMulticast == false &&
           (request->devAddr.adapter != OC_ADAPTER_RFCOMM_BTEDR) &&
           (request->devAddr.adapter != OC_ADAPTER_GATT_BTLE));
}

static OCStackResult HandleVirtualResource (OCServerRequest *request, OCResource* resource)
{
    if (!request || !resource)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCPayload* payload = NULL;
    char *interfaceQuery = NULL;
    char *resourceTypeQuery = NULL;

    OIC_LOG(INFO, TAG, "Entering HandleVirtualResource");

    OCVirtualResources virtualUriInRequest = GetTypeOfVirtualURI (request->resourceUrl);

#ifdef TCP_ADAPTER
    if (OC_KEEPALIVE_RESOURCE_URI == virtualUriInRequest)
    {
        // Received request for a keepalive
        OIC_LOG(INFO, TAG, "Request is for KeepAlive Request");
        return HandleKeepAliveRequest(request, resource);
    }
#endif

    OCStackResult discoveryResult = OC_STACK_ERROR;
    if (request->method == OC_REST_PUT || request->method == OC_REST_POST ||
        request->method == OC_REST_DELETE)
    {
        OIC_LOG_V(ERROR, TAG, "Resource : %s not permitted for method: %d",
            request->resourceUrl, request->method);
        return OC_STACK_UNAUTHORIZED_REQ;
    }

    discoveryResult = HandleVirtualObserveRequest(request);
    if (discoveryResult == OC_STACK_DUPLICATE_REQUEST)
    {
        // server requests are usually free'd when the response is sent out
        // for the request in ocserverrequest.c : HandleSingleResponse()
        // Since we are making an early return and not responding, the server request
        // needs to be deleted.
        FindAndDeleteServerRequest (request);
        discoveryResult = OC_STACK_OK;
        goto exit;
    }
    else if (discoveryResult != OC_STACK_OK)
    {
        goto exit;
    }

    // Step 1: Generate the response to discovery request
    if (virtualUriInRequest == OC_WELL_KNOWN_URI
#ifdef MQ_BROKER
            || virtualUriInRequest == OC_MQ_BROKER_URI
#endif
            )
    {
        char *interfaceQuery = NULL;
        char *resourceTypeQuery = NULL;

        CAEndpoint_t *networkInfo = NULL;
        size_t infoSize = 0;

        CAResult_t caResult = CAGetNetworkInformation(&networkInfo, &infoSize);
        if (CA_STATUS_FAILED == caResult)
        {
            OIC_LOG(ERROR, TAG, "CAGetNetworkInformation has error on parsing network infomation");
            return OC_STACK_ERROR;
        }

        discoveryResult = getQueryParamsForFiltering (virtualUriInRequest, request->query,
                &interfaceQuery, &resourceTypeQuery);
        VERIFY_SUCCESS(discoveryResult);

        if (!interfaceQuery && !resourceTypeQuery)
        {
            // If no query is sent, default interface is used i.e. oic.if.ll.
            interfaceQuery = OICStrdup(OC_RSRVD_INTERFACE_LL);
        }

        discoveryResult = discoveryPayloadCreateAndAddDeviceId(&payload);
        VERIFY_PARAM_NON_NULL(TAG, payload, "Failed creating Discovery Payload.");
        VERIFY_SUCCESS(discoveryResult);

        OCDiscoveryPayload *discPayload = (OCDiscoveryPayload *)payload;
        if (interfaceQuery && 0 == strcmp(interfaceQuery, OC_RSRVD_INTERFACE_DEFAULT))
        {
            discoveryResult = addDiscoveryBaselineCommonProperties(discPayload);
            VERIFY_SUCCESS(discoveryResult);
        }
        OCResourceProperty prop = OC_DISCOVERABLE;
#ifdef MQ_BROKER
        prop = (OC_MQ_BROKER_URI == virtualUriInRequest) ? OC_MQ_BROKER : prop;
#endif
        for (; resource && discoveryResult == OC_STACK_OK; resource = resource->next)
        {
            // This case will handle when no resource type and it is oic.if.ll.
            // Do not assume check if the query is ll
            if (!resourceTypeQuery &&
                (interfaceQuery && 0 == strcmp(interfaceQuery, OC_RSRVD_INTERFACE_LL)))
            {
                // Only include discoverable type
                if (resource->resourceProperties & prop)
                {
                    discoveryResult = BuildVirtualResourceResponse(resource,
                                                                   discPayload,
                                                                   &request->devAddr,
                                                                   networkInfo,
                                                                   infoSize);
                }
            }
            else if (includeThisResourceInResponse(resource, interfaceQuery, resourceTypeQuery))
            {
                discoveryResult = BuildVirtualResourceResponse(resource,
                                                               discPayload,
                                                               &request->devAddr,
                                                               networkInfo,
                                                               infoSize);
            }
            else
            {
                discoveryResult = OC_STACK_OK;
            }
        }
        if (discPayload->resources == NULL)
        {
            discoveryResult = OC_STACK_NO_RESOURCE;
            OCPayloadDestroy(payload);
            payload = NULL;
        }

        if (networkInfo)
        {
            OICFree(networkInfo);
        }
#ifdef RD_SERVER
        discoveryResult = findResourcesAtRD(interfaceQuery, resourceTypeQuery, (OCDiscoveryPayload **)&payload);
#endif
    }
    else if (virtualUriInRequest == OC_DEVICE_URI)
    {
        OCResource *resourcePtr = FindResourceByUri(OC_RSRVD_DEVICE_URI);
        VERIFY_PARAM_NON_NULL(TAG, resourcePtr, "Device URI not found.");
        discoveryResult = BuildDevicePlatformPayload(resourcePtr, (OCRepPayload **)&payload, true);
    }
    else if (virtualUriInRequest == OC_PLATFORM_URI)
    {
        OCResource *resourcePtr = FindResourceByUri(OC_RSRVD_PLATFORM_URI);
        VERIFY_PARAM_NON_NULL(TAG, resourcePtr, "Platform URI not found.");
        discoveryResult = BuildDevicePlatformPayload(resourcePtr, (OCRepPayload **)&payload, false);
    }
#ifdef ROUTING_GATEWAY
    else if (OC_GATEWAY_URI == virtualUriInRequest)
    {
        // Received request for a gateway
        OIC_LOG(INFO, TAG, "Request is for Gateway Virtual Request");
        discoveryResult = RMHandleGatewayRequest(request, resource);
    }
#endif
    else if (OC_INTROSPECTION_URI == virtualUriInRequest)
    {
        // Received request for introspection
        OCResource *resourcePtr = FindResourceByUri(OC_RSRVD_INTROSPECTION_URI);
        VERIFY_PARAM_NON_NULL(TAG, resourcePtr, "Introspection URI not found.");
        discoveryResult = BuildIntrospectionResponseRepresentation(resourcePtr, (OCRepPayload **)&payload, &request->devAddr);
        OIC_LOG(INFO, TAG, "Request is for Introspection");
    }
    else if (OC_INTROSPECTION_PAYLOAD_URI == virtualUriInRequest)
    {
        // Received request for introspection payload
        OCResource *resourcePtr = FindResourceByUri(OC_RSRVD_INTROSPECTION_PAYLOAD_URI);
        VERIFY_PARAM_NON_NULL(TAG, resourcePtr, "Introspection Payload URI not found.");
        discoveryResult = BuildIntrospectionPayloadResponse(resourcePtr, (OCRepPayload **)&payload, &request->devAddr);
        OIC_LOG(INFO, TAG, "Request is for Introspection Payload");
    }
    /**
     * Step 2: Send the discovery response
     *
     * Iotivity should respond to discovery requests in below manner:
     * 1)If query filter matching fails and discovery request is multicast,
     *   it should NOT send any response.
     * 2)If query filter matching fails and discovery request is unicast,
     *   it should send an error(RESOURCE_NOT_FOUND - 404) response.
     * 3)If Server does not have any 'DISCOVERABLE' resources and discovery
     *   request is multicast, it should NOT send any response.
     * 4)If Server does not have any 'DISCOVERABLE' resources and discovery
     *   request is unicast, it should send an error(RESOURCE_NOT_FOUND - 404) response.
     */

#ifdef WITH_PRESENCE
    if ((virtualUriInRequest == OC_PRESENCE) &&
        (resource->resourceProperties & OC_ACTIVE))
    {
        // Need to send ACK when the request is CON.
        if (request->qos == OC_HIGH_QOS)
        {
            CAEndpoint_t endpoint = { .adapter = CA_DEFAULT_ADAPTER };
            CopyDevAddrToEndpoint(&request->devAddr, &endpoint);
            SendDirectStackResponse(&endpoint, request->coapID, CA_EMPTY, CA_MSG_ACKNOWLEDGE,
                                    0, NULL, NULL, 0, NULL, CA_RESPONSE_FOR_RES);
        }
        FindAndDeleteServerRequest(request);

        // Presence uses observer notification api to respond via SendPresenceNotification.
        SendPresenceNotification(resource->rsrcType, OC_PRESENCE_TRIGGER_CHANGE);
    }
    else
#endif
#if ROUTING_GATEWAY
    // Gateway uses the RMHandleGatewayRequest to respond to the request.
    if (OC_GATEWAY_URI != virtualUriInRequest)
#endif
    {
        OIC_LOG_PAYLOAD(DEBUG, payload);
        if(discoveryResult == OC_STACK_OK)
        {

            SendNonPersistantDiscoveryResponse(request, resource, payload, OC_EH_OK);
        }
        else // Error handling
        {
            if (isUnicast(request))
            {
                OIC_LOG_V(ERROR, TAG, "Sending a (%d) error to (%d) discovery request",
                    discoveryResult, virtualUriInRequest);
                SendNonPersistantDiscoveryResponse(request, resource, NULL,
                    (discoveryResult == OC_STACK_NO_RESOURCE) ?
                        OC_EH_RESOURCE_NOT_FOUND : OC_EH_ERROR);
            }
            else // Multicast
            {
                // Ignoring the discovery request as per RFC 7252, Section #8.2
                OIC_LOG(INFO, TAG, "Silently ignoring the request since no useful data to send.");
                // the request should be removed.
                // since it never remove and causes a big memory waste.
                FindAndDeleteServerRequest(request);
            }
            discoveryResult = OC_STACK_CONTINUE;
        }
    }

exit:
    if (interfaceQuery)
    {
        OICFree(interfaceQuery);
    }

    if (resourceTypeQuery)
    {
        OICFree(resourceTypeQuery);
    }
    OCPayloadDestroy(payload);

    // To ignore the message, OC_STACK_CONTINUE is sent
    return discoveryResult;
}

static OCStackResult
HandleDefaultDeviceEntityHandler(OCServerRequest *request)
{
    if (!request)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCEntityHandlerResult ehResult = OC_EH_ERROR;
    OCEntityHandlerRequest ehRequest = {0};
    OIC_LOG(INFO, TAG, "Entering HandleResourceWithDefaultDeviceEntityHandler");
    OCStackResult result = EHRequest(&ehRequest, PAYLOAD_TYPE_REPRESENTATION, request, NULL);
    VERIFY_SUCCESS(result);

    // At this point we know for sure that defaultDeviceHandler exists
    ehResult = defaultDeviceHandler(OC_REQUEST_FLAG, &ehRequest,
                                  (char*) request->resourceUrl, defaultDeviceHandlerCallbackParameter);
    if(ehResult == OC_EH_SLOW)
    {
        OIC_LOG(INFO, TAG, "This is a slow resource");
        request->slowFlag = 1;
    }
    else if(ehResult == OC_EH_ERROR)
    {
        FindAndDeleteServerRequest(request);
    }
    result = EntityHandlerCodeToOCStackCode(ehResult);
exit:
    OCPayloadDestroy(ehRequest.payload);
    return result;
}

static OCStackResult
HandleResourceWithEntityHandler(OCServerRequest *request,
                                OCResource *resource)
{
    if(!request || ! resource)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCStackResult result = OC_STACK_ERROR;
    OCEntityHandlerResult ehResult = OC_EH_ERROR;
    OCEntityHandlerFlag ehFlag = OC_REQUEST_FLAG;
    ResourceObserver *resObs = NULL;

    OCEntityHandlerRequest ehRequest = {0};

    OIC_LOG(INFO, TAG, "Entering HandleResourceWithEntityHandler");
    OCPayloadType type = PAYLOAD_TYPE_REPRESENTATION;
    // check the security resource
    if (request && request->resourceUrl && SRMIsSecurityResourceURI(request->resourceUrl))
    {
        type = PAYLOAD_TYPE_SECURITY;

    }

    result = EHRequest(&ehRequest, type, request, resource);
    VERIFY_SUCCESS(result);

    if(ehRequest.obsInfo.action == OC_OBSERVE_NO_OPTION)
    {
        OIC_LOG(INFO, TAG, "No observation requested");
        ehFlag = OC_REQUEST_FLAG;
    }
    else if(ehRequest.obsInfo.action == OC_OBSERVE_REGISTER)
    {
        OIC_LOG(INFO, TAG, "Observation registration requested");

        ResourceObserver *obs = GetObserverUsingToken (request->requestToken,
                                    request->tokenLength);

        if (obs)
        {
            OIC_LOG (INFO, TAG, "Observer with this token already present");
            OIC_LOG (INFO, TAG, "Possibly re-transmitted CON OBS request");
            OIC_LOG (INFO, TAG, "Not adding observer. Not responding to client");
            OIC_LOG (INFO, TAG, "The first request for this token is already ACKED.");

            // server requests are usually free'd when the response is sent out
            // for the request in ocserverrequest.c : HandleSingleResponse()
            // Since we are making an early return and not responding, the server request
            // needs to be deleted.
            FindAndDeleteServerRequest (request);
            return OC_STACK_OK;
        }

        result = GenerateObserverId(&ehRequest.obsInfo.obsId);
        VERIFY_SUCCESS(result);

        result = AddObserver ((const char*)(request->resourceUrl),
                (const char *)(request->query),
                ehRequest.obsInfo.obsId, request->requestToken, request->tokenLength,
                resource, request->qos, request->acceptFormat,
                request->acceptVersion, &request->devAddr);

        if(result == OC_STACK_OK)
        {
            OIC_LOG(INFO, TAG, "Added observer successfully");
            request->observeResult = OC_STACK_OK;
            ehFlag = (OCEntityHandlerFlag)(OC_REQUEST_FLAG | OC_OBSERVE_FLAG);
        }
        else if (result == OC_STACK_RESOURCE_ERROR)
        {
            OIC_LOG(INFO, TAG, "The Resource is not active, discoverable or observable");
            request->observeResult = OC_STACK_ERROR;
            ehFlag = OC_REQUEST_FLAG;
        }
        else
        {
            // The error in observeResult for the request will be used when responding to this
            // request by omitting the observation option/sequence number.
            request->observeResult = OC_STACK_ERROR;
            OIC_LOG(ERROR, TAG, "Observer Addition failed");
            ehFlag = OC_REQUEST_FLAG;
            FindAndDeleteServerRequest(request);
            goto exit;
        }

    }
    else if(ehRequest.obsInfo.action == OC_OBSERVE_DEREGISTER)
    {
        OIC_LOG(INFO, TAG, "Deregistering observation requested");

        resObs = GetObserverUsingToken (request->requestToken, request->tokenLength);

        if (NULL == resObs)
        {
            // Stack does not contain this observation request
            // Either token is incorrect or observation list is corrupted
            result = OC_STACK_ERROR;
            goto exit;
        }
        ehRequest.obsInfo.obsId = resObs->observeId;
        ehFlag = (OCEntityHandlerFlag)(ehFlag | OC_OBSERVE_FLAG);

        result = DeleteObserverUsingToken (request->requestToken, request->tokenLength);

        if(result == OC_STACK_OK)
        {
            OIC_LOG(INFO, TAG, "Removed observer successfully");
            request->observeResult = OC_STACK_OK;
            // There should be no observe option header for de-registration response.
            // Set as an invalid value here so we can detect it later and remove the field in response.
            request->observationOption = MAX_SEQUENCE_NUMBER + 1;
        }
        else
        {
            request->observeResult = OC_STACK_ERROR;
            OIC_LOG(ERROR, TAG, "Observer Removal failed");
            FindAndDeleteServerRequest(request);
            goto exit;
        }
    }
    else
    {
        result = OC_STACK_ERROR;
        goto exit;
    }

    ehResult = resource->entityHandler(ehFlag, &ehRequest, resource->entityHandlerCallbackParam);
    if(ehResult == OC_EH_SLOW)
    {
        OIC_LOG(INFO, TAG, "This is a slow resource");
        request->slowFlag = 1;
    }
    else if(ehResult == OC_EH_ERROR)
    {
        FindAndDeleteServerRequest(request);
    }
    result = EntityHandlerCodeToOCStackCode(ehResult);
exit:
    OCPayloadDestroy(ehRequest.payload);
    return result;
}

static OCStackResult HandleCollectionResourceDefaultEntityHandler(OCServerRequest *request,
                                                                  OCResource *resource)
{
    if (!request || !resource)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCEntityHandlerRequest ehRequest = {0};
    OCStackResult result = EHRequest(&ehRequest, PAYLOAD_TYPE_REPRESENTATION, request, resource);
    if(result == OC_STACK_OK)
    {
        result = DefaultCollectionEntityHandler (OC_REQUEST_FLAG, &ehRequest);
    }

    OCPayloadDestroy(ehRequest.payload);
    return result;
}

OCStackResult
ProcessRequest(ResourceHandling resHandling, OCResource *resource, OCServerRequest *request)
{
    OCStackResult ret = OC_STACK_OK;

    switch (resHandling)
    {
        case OC_RESOURCE_VIRTUAL:
        {
            ret = HandleVirtualResource (request, resource);
            break;
        }
        case OC_RESOURCE_DEFAULT_DEVICE_ENTITYHANDLER:
        {
            ret = HandleDefaultDeviceEntityHandler(request);
            break;
        }
        case OC_RESOURCE_NOT_COLLECTION_DEFAULT_ENTITYHANDLER:
        {
            OIC_LOG(INFO, TAG, "OC_RESOURCE_NOT_COLLECTION_DEFAULT_ENTITYHANDLER");
            return OC_STACK_ERROR;
        }
        case OC_RESOURCE_NOT_COLLECTION_WITH_ENTITYHANDLER:
        {
            ret = HandleResourceWithEntityHandler (request, resource);
            break;
        }
        case OC_RESOURCE_COLLECTION_WITH_ENTITYHANDLER:
        {
            ret = HandleResourceWithEntityHandler (request, resource);
            break;
        }
        case OC_RESOURCE_COLLECTION_DEFAULT_ENTITYHANDLER:
        {
            ret = HandleCollectionResourceDefaultEntityHandler (request, resource);
            break;
        }
        case OC_RESOURCE_NOT_SPECIFIED:
        {
            ret = OC_STACK_NO_RESOURCE;
            break;
        }
        default:
        {
            OIC_LOG(INFO, TAG, "Invalid Resource Determination");
            return OC_STACK_ERROR;
        }
    }
    return ret;
}

OCStackResult OCSetPlatformInfo(OCPlatformInfo info)
{
    OCResource *resource = NULL;
    if (!info.platformID || !info.manufacturerName)
    {
        OIC_LOG(ERROR, TAG, "No value specified.");
        goto exit;
    }
    if (0 == strlen(info.platformID) || 0 == strlen(info.manufacturerName))
    {
        OIC_LOG(ERROR, TAG, "The passed value cannot be empty");
        goto exit;
    }
    if ((info.manufacturerName && strlen(info.manufacturerName) > MAX_PLATFORM_NAME_LENGTH) ||
        (info.manufacturerUrl && strlen(info.manufacturerUrl) > MAX_PLATFORM_URL_LENGTH) ||
        (info.modelNumber && strlen(info.modelNumber) > MAX_PLATFORM_NAME_LENGTH) ||
        (info.platformVersion && strlen(info.platformVersion) > MAX_PLATFORM_NAME_LENGTH) ||
        (info.operatingSystemVersion && strlen(info.operatingSystemVersion) > MAX_PLATFORM_NAME_LENGTH) ||
        (info.hardwareVersion && strlen(info.hardwareVersion) > MAX_PLATFORM_NAME_LENGTH) ||
        (info.firmwareVersion && strlen(info.firmwareVersion) > MAX_PLATFORM_NAME_LENGTH) ||
        (info.supportUrl && strlen(info.supportUrl) > MAX_PLATFORM_URL_LENGTH))
    {
        OIC_LOG(ERROR, TAG, "The passed value is bigger than permitted.");
        goto exit;
    }

    /*
     * @todo (IOT-1541) There are several versions of a UUID structure and conversion
     * methods scattered around the IoTivity code base.  They need to be combined
     * into one PAL API.
     */
    uint8_t uuid[UUID_SIZE];
    if (!OCConvertStringToUuid(info.platformID, uuid))
    {
        OIC_LOG(ERROR, TAG, "Platform ID is not a UUID.");
        goto exit;
    }

    resource = FindResourceByUri(OC_RSRVD_PLATFORM_URI);
    if (!resource)
    {
        OIC_LOG(ERROR, TAG, "Platform Resource does not exist.");
        goto exit;
    }
    OIC_LOG(INFO, TAG, "Entering OCSetPlatformInfo");
    VERIFY_SUCCESS(OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, info.platformID));
    VERIFY_SUCCESS(OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME, info.manufacturerName));
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, info.manufacturerUrl);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MODEL_NUM, info.modelNumber);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, info.dateOfManufacture);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, info.platformVersion);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, info.operatingSystemVersion);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, info.hardwareVersion);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, info.firmwareVersion);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, info.supportUrl);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, info.systemTime);
    OIC_LOG(INFO, TAG, "Platform parameter initialized successfully.");
    return OC_STACK_OK;

exit:
    return OC_STACK_INVALID_PARAM;
}

OCStackResult OCSetDeviceInfo(OCDeviceInfo info)
{
    OCResource *resource = FindResourceByUri(OC_RSRVD_DEVICE_URI);
    if (!resource)
    {
        OIC_LOG(ERROR, TAG, "Device Resource does not exist.");
        goto exit;
    }
    if (!info.deviceName || info.deviceName[0] == '\0')
    {
        OIC_LOG(ERROR, TAG, "Null or empty device name.");
       return OC_STACK_INVALID_PARAM;
    }

    if (OCGetServerInstanceIDString() == NULL)
    {
        OIC_LOG(INFO, TAG, "Device ID generation failed");
        goto exit;
    }

    VERIFY_SUCCESS(OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, info.deviceName));
    for (OCStringLL *temp = info.types; temp; temp = temp->next)
    {
        if (temp->value)
        {
            VERIFY_SUCCESS(OCBindResourceTypeToResource(resource, temp->value));
        }
    }
    VERIFY_SUCCESS(OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SPEC_VERSION, info.specVersion ?
        info.specVersion: OC_SPEC_VERSION));

    if (info.dataModelVersions)
    {
        char *dmv = OCCreateString(info.dataModelVersions);
        VERIFY_PARAM_NON_NULL(TAG, dmv, "Failed allocating dataModelVersions");
        OCStackResult r = OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION, dmv);
        OICFree(dmv);
        VERIFY_SUCCESS(r);
    }
    else
    {
        VERIFY_SUCCESS(OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION,
            OC_DATA_MODEL_VERSION));
    }
    OIC_LOG(INFO, TAG, "Device parameter initialized successfully.");
    return OC_STACK_OK;

exit:
    return OC_STACK_ERROR;
}

OCStackResult OCGetAttribute(const OCResource *resource, const char *attribute, void **value)
{
    if (!resource || !attribute)
    {
        return OC_STACK_INVALID_PARAM;
    }
    if (0 == strlen(attribute))
    {
        return OC_STACK_INVALID_PARAM;
    }
    for (OCAttribute *temp = resource->rsrcAttributes; temp; temp = temp->next)
    {
        if (0 == strcmp(attribute, temp->attrName))
        {
            // A special case as this type return OCStringLL
            if (0 == strcmp(OC_RSRVD_DATA_MODEL_VERSION, attribute))
            {
                *value = CloneOCStringLL((OCStringLL *)temp->attrValue);
                return OC_STACK_OK;
            }
            else
            {
                *value = OICStrdup((char *)temp->attrValue);
                return OC_STACK_OK;
            }
        }
    }
    return OC_STACK_NO_RESOURCE;
}

OCStackResult OCGetPropertyValue(OCPayloadType type, const char *prop, void **value)
{
    if (!prop)
    {
        return OC_STACK_INVALID_PARAM;
    }
    if (strlen(prop) == 0)
    {
        return OC_STACK_INVALID_PARAM;
    }
    if (*value)
    {
        *value = NULL;
    }
    OCStackResult res =  OC_STACK_NO_RESOURCE;
    if (PAYLOAD_TYPE_DEVICE == type || PAYLOAD_TYPE_PLATFORM == type)
    {
        const char *pathType = (type == PAYLOAD_TYPE_DEVICE) ? OC_RSRVD_DEVICE_URI : OC_RSRVD_PLATFORM_URI;
        OCResource *resource = FindResourceByUri(pathType);
        if (!resource)
        {
            return OC_STACK_NO_RESOURCE;
        }

        res = OCGetAttribute(resource, prop, value);
    }
    return res;
}

OCStackResult OCSetAttribute(OCResource* resource, const char* attribute, const void* value)
{
    // See if the attribute already exists in the list.
    OCAttribute *resAttrib = NULL;
    for (resAttrib = resource->rsrcAttributes; resAttrib; resAttrib = resAttrib->next)
    {
        if (0 == strcmp(attribute, resAttrib->attrName))
        {
            // Found, free the old value.
            if (0 == strcmp(OC_RSRVD_DATA_MODEL_VERSION, resAttrib->attrName))
            {
                OCFreeOCStringLL((OCStringLL *)resAttrib->attrValue);
            }
            else
            {
                OICFree((char *)resAttrib->attrValue);
            }
            break;
        }
    }

    // If not already in the list, add it.
    if (NULL == resAttrib)
    {
        resAttrib = (OCAttribute *)OICCalloc(1, sizeof(OCAttribute));
        VERIFY_PARAM_NON_NULL(TAG, resAttrib, "Failed allocating OCAttribute");
        resAttrib->attrName = OICStrdup(attribute);
        VERIFY_PARAM_NON_NULL(TAG, resAttrib->attrName, "Failed allocating attribute name");
        resAttrib->next = resource->rsrcAttributes;
        resource->rsrcAttributes = resAttrib;
    }

    // Fill in the new value.
    if (0 == strcmp(OC_RSRVD_DATA_MODEL_VERSION, attribute))
    {
        resAttrib->attrValue = OCCreateOCStringLL((char *)value);
    }
    else
    {
        resAttrib->attrValue = OICStrdup((char *)value);
    }
    VERIFY_PARAM_NON_NULL(TAG, resAttrib->attrValue, "Failed allocating attribute value");

    return OC_STACK_OK;

exit:
    OCDeleteResourceAttributes(resAttrib);
    return OC_STACK_NO_MEMORY;

}

OCStackResult OCSetPropertyValue(OCPayloadType type, const char *prop, const void *value)
{
    if (!prop || !value)
    {
        return OC_STACK_INVALID_PARAM;
    }
    if (strlen(prop) == 0)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCStackResult res = OC_STACK_ERROR;
    if (PAYLOAD_TYPE_DEVICE == type || PAYLOAD_TYPE_PLATFORM == type)
    {
        const char *pathType = (type == PAYLOAD_TYPE_DEVICE) ? OC_RSRVD_DEVICE_URI : OC_RSRVD_PLATFORM_URI;
        OCResource *resource = FindResourceByUri(pathType);
        if (!resource)
        {
            OIC_LOG(ERROR, TAG, "Resource does not exist.");
        }
        else
        {
            res = OCSetAttribute(resource, prop, value);
        }
    }

    return res;
}

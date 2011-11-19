/******************************************************************************
 * $Id $
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_http.h"

/************************************************************************/
/*                          OGRElasticDataSource()                          */

/************************************************************************/

OGRElasticDataSource::OGRElasticDataSource() {
    papoLayers = NULL;
    nLayers = 0;
    pszName = NULL;
    bUseExtensions = FALSE;
    bWriteHeaderAndFooter = TRUE;
}

/************************************************************************/
/*                         ~OGRElasticDataSource()                          */

/************************************************************************/

OGRElasticDataSource::~OGRElasticDataSource() {
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    CPLFree(pszName);
}

/************************************************************************/
/*                           TestCapability()                           */

/************************************************************************/

int OGRElasticDataSource::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */

/************************************************************************/

OGRLayer *OGRElasticDataSource::GetLayer(int iLayer) {
    if (iLayer < 0 || iLayer >= nLayers)
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            CreateLayer()                             */

/************************************************************************/

OGRLayer * OGRElasticDataSource::CreateLayer(const char * pszLayerName,
        OGRSpatialReference *poSRS,
        OGRwkbGeometryType eType,
        char ** papszOptions) {
    nLayers++;
    papoLayers = (OGRElasticLayer **) CPLRealloc(papoLayers, nLayers * sizeof (OGRElasticLayer*));
    papoLayers[nLayers - 1] = new OGRElasticLayer(pszName, pszLayerName, this, poSRS, TRUE);

    return papoLayers[nLayers - 1];
}

/************************************************************************/
/*                                Open()                                */

/************************************************************************/

int OGRElasticDataSource::Open(const char * pszFilename, int bUpdateIn) {
    CPLError(CE_Failure, CPLE_NotSupported,
            "OGR/Elastic driver does not support opening a file");
    return FALSE;
}


/************************************************************************/
/*                               Create()                               */

/************************************************************************/

void OGRElasticDataSource::DeleteIndex(const CPLString &url) {
    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST", "DELETE");
    CPLHTTPResult* psResult = CPLHTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if (psResult) {
        CPLHTTPDestroyResult(psResult);
    }
}

void OGRElasticDataSource::UploadFile(const CPLString &url, const CPLString &data) {
    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", data.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
            "Content-Type: application/x-javascript; charset=UTF-8");

    CPLHTTPResult* psResult = CPLHTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if (psResult) {
        CPLHTTPDestroyResult(psResult);
    }
}

int OGRElasticDataSource::Create(const char *pszFilename,
        char **papszOptions) {
    this->psMapping = NULL;
    this->psWriteMap = CPLGetConfigOption("ES_WRITEMAP", NULL);
    this->psMetaFile = CPLGetConfigOption("ES_META", NULL);
    this->bOverwrite = (int) CPLAtof(CPLGetConfigOption("ES_OVERWRITE", "0"));
    this->nBulkUpload = (int) CPLAtof(CPLGetConfigOption("ES_BULK", "0"));

    // Read in the meta file from disk
    if (this->psMetaFile != NULL) {
        int fsize;
        char *fdata;
        FILE *fp;

        fp = fopen(this->psMetaFile, "rb");
        if (fp != NULL) {
            fseek(fp, 0, SEEK_END);
            fsize = (int) ftell(fp);

            fdata = (char *) malloc(fsize + 1);

            fseek(fp, 0, SEEK_SET);
            fread(fdata, fsize, 1, fp);
            fdata[fsize] = 0;
            this->psMapping = fdata;
        }
    }

    // Do a status check to ensure that the server is valid
    CPLHTTPResult* psResult = CPLHTTPFetch(CPLSPrintf("%s/_status", pszFilename), NULL);
    if (psResult) {
        CPLHTTPDestroyResult(psResult);
    } else {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                "Could not connect to server");
        return FALSE;
    }

    pszName = CPLStrdup(pszFilename);
    return TRUE;
}

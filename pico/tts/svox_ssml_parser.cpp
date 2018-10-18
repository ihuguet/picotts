/*
 * Copyright (C) 2008-2009 SVOX AG, Baslerstr. 30, 8048 Zuerich, Switzerland
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
*/

#include "svox_ssml_parser.h"
#include <utils/Log.h>
#include <cutils/jstring.h>
#include <string.h>
#include <utils/String16.h>

#define SSML_PITCH_XLOW     "50"
#define SSML_PITCH_LOW      "75"
#define SSML_PITCH_MEDIUM   "100"
#define SSML_PITCH_HIGH     "150"
#define SSML_PITCH_XHIGH    "200"
#define SSML_RATE_XSLOW     "30"
#define SSML_RATE_SLOW      "60"
#define SSML_RATE_MEDIUM    "100"
#define SSML_RATE_FAST      "250"
#define SSML_RATE_XFAST     "500"
#define SSML_VOLUME_SILENT  "0"
#define SSML_VOLUME_XLOW    "25"
#define SSML_VOLUME_LOW     "70"
#define SSML_VOLUME_MEDIUM  "120"
#define SSML_VOLUME_LOUD    "300"
#define SSML_VOLUME_XLOUD   "450"
#define SSML_BREAK_NONE     "0ms"
#define SSML_BREAK_XWEAK    "100ms"
#define SSML_BREAK_WEAK     "300ms"
#define SSML_BREAK_MEDIUM   "600ms"
#define SSML_BREAK_STRONG   "1s"
#define SSML_BREAK_XSTRONG  "3s"

extern int cnvIpaToXsampa(const char16_t* ipaString, size_t ipaStringSize, char** outXsampaString);
extern char * createPhonemeString( const char * xsampa, int length );

SvoxSsmlParser::SvoxSsmlParser() : m_isInBreak(0), m_appendix(NULL), m_docLanguage(NULL)
{
    mParser = XML_ParserCreate("UTF-8");
    if (mParser)
    {
        XML_SetElementHandler(mParser, starttagHandler, endtagHandler);
        XML_SetCharacterDataHandler(mParser, textHandler);
        XML_SetUserData(mParser, (void*)this);
        m_datasize = 512;
        m_data = new char[m_datasize];
        m_data[0] = '\0';
    }
}

SvoxSsmlParser::~SvoxSsmlParser()
{
    if (mParser)
        XML_ParserFree(mParser);
    if (m_data)
        delete [] m_data;
    if (m_appendix)
        delete [] m_appendix;
    if (m_docLanguage)
        delete [] m_docLanguage;
}

int SvoxSsmlParser::initSuccessful()
{
    return (mParser && m_data);
}

int SvoxSsmlParser::parseDocument(const char* ssmldoc, int isFinal)
{
    int doclen = (int)strlen(ssmldoc) + 1;
    int status = XML_Parse(mParser, ssmldoc, doclen, isFinal);
    if (status == XML_STATUS_ERROR)
    {
        /* Note: for some reason Expat almost always complains about invalid tokens, even when document is well formed */
        ALOGI("Parser error at line %d: %s\n", (int)XML_GetCurrentLineNumber(mParser), XML_ErrorString(XML_GetErrorCode(mParser)));
    }
    return status;
}

char* SvoxSsmlParser::getParsedDocument()
{
    return m_data;
}

char* SvoxSsmlParser::getParsedDocumentLanguage()
{
    return m_docLanguage;
}

void SvoxSsmlParser::starttagHandler(void* data, const XML_Char* element, const XML_Char** attributes)
{
    ((SvoxSsmlParser*)data)->startElement(element, attributes);
}

void SvoxSsmlParser::startElement(const XML_Char* element, const XML_Char** attributes)
{
    if (strcmp(element, "speak") == 0)
    {
        if (strlen(m_data) > 0)
        {
            /* we have old data, get rid of it and reallocate memory */
            delete m_data;
            m_data = NULL;
            m_datasize = 512;
            m_data = new char[m_datasize];
            if (!m_data)
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }

        /* the only attribute supported in the speak tag is xml:lang, all others are ignored */
        for (int i = 0; attributes[i]; i += 2)
        {
            if (strcmp(attributes[i], "xml:lang") == 0)
            {
                if (!m_docLanguage)
                {
                    m_docLanguage = new char[strlen(attributes[i+1])+1];
                }
                strcpy(m_docLanguage, attributes[i+1]);
                break;
            }
        }
    }
    else if (strcmp(element, "p") == 0) /* currently no attributes are supported for <p> */
    {
        if (strlen(m_data) + 4 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "<p>");
    }
    else if (strcmp(element, "s") == 0) /* currently no attributes are supported for <s> */
    {
        if (strlen(m_data) + 4 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "<s>");
    }
    else if (strcmp(element, "phoneme") == 0) /* only ipa and xsampa alphabets are supported */
    {
        int alpha = 1; /* set to 1 if alphabet is ipa */
        int tagComplete = 0; /* set to 1 if phoneme tag has already been added */
    char16_t* ph = NULL;
        char* xsampastr = NULL;
    size_t phsize = 0;
    size_t xsampasize = 0;

        for (int i = 0; attributes[i]; i += 2)
        {
            if (strcmp(attributes[i], "alphabet") == 0)
            {
                if (strcmp(attributes[i+1], "xsampa") == 0)
                {
                    alpha = 0;
                }
            }
            if (strcmp(attributes[i], "ph") == 0)
            {
          ph = new char16_t[strlen8to16(attributes[i+1]) + 1];
          ph = strdup8to16(attributes[i+1], &phsize);
            }
        }
        if (!ph)
        {
            /* error, no phonetic string */
            ALOGE("Error: bad SSML syntax, ph attribute not supplied.");
            return;
        }

        if (alpha)
        {
          /* need to convert phoneme string to xsampa */
      xsampasize = cnvIpaToXsampa(ph, phsize, &xsampastr);
      delete [] ph;
      if (!xsampastr)
            {
                ALOGE("Error: failed to allocate memory for IPA string conversion");
                return;
            }
        }
        else
        {
      xsampastr = strndup16to8(ph, phsize);
      xsampasize = strlen(xsampastr);
          delete [] ph;
        }

        /* split XSAMPA string into multiple phonemes if needed */
        if (strstr(xsampastr, " ") || strstr(xsampastr, "#")) /* check again to see if we have multiple words */
        {
            char* phonstr = createPhonemeString(xsampastr, strlen(xsampastr) + 1);
            free(xsampastr);
            xsampastr = NULL;
            xsampastr = (char*)malloc(strlen(phonstr) + 1);
            strcpy(xsampastr, phonstr);
            free(phonstr);
            phonstr = NULL;
            tagComplete = 1;
        }

        if (tagComplete)
        {
            if (strlen(m_data) + strlen(xsampastr) + 1 > (size_t)m_datasize)
            {
                if (!growDataSize(100))
                {
                    ALOGE("Error: failed to allocate memory for string!");
                    free(xsampastr);
                    return;
                }
            }
        }
        else
        {
            if (strlen(m_data) + strlen(xsampastr) + 17 > (size_t)m_datasize)
            {
                if (!growDataSize(100))
                {
                    ALOGE("Error: failed to allocate memory for string!");
                    free(xsampastr);
                    return;
                }
            }
            strcat(m_data, "<phoneme ph='");
        }

        strcat(m_data, xsampastr);
        free(xsampastr);

    if (!tagComplete)
      {
        if (strlen(m_data) + 4 > (size_t)m_datasize)
          {
        if (!growDataSize(100))
          {
            ALOGE("Error: failed to allocate memory for string!\n");
            return;
          }
          }
        strcat(m_data, "'/>");
      }

        m_isInBreak = 1; /* set flag to indicate any text between open and close tag is to be discarded */
    }
    else if (strcmp(element, "break") == 0)
    {
        if (strlen(m_data) + 17 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "<break time='");
        char* time = NULL;

        for (int i = 0; attributes[i]; i += 2)
        {
            if (strcmp(attributes[i], "time") == 0)
            {
                time = new char[strlen(attributes[i+1]) + 1];
                if (!time)
                {
                    ALOGE("Error: failed to allocate memory for string!\n");
                    return;
                }
                strcpy(time, attributes[i+1]);
            }
            else if (strcmp(attributes[i], "strength") == 0 && !time)
            {
                time = convertBreakStrengthToTime(attributes[i+1]);
            }
        }
        if (!time)
        {
            time = new char[6];
            if (!time)
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
            strcpy(time, SSML_BREAK_WEAK); /* if no time or strength attributes are specified, default to weak break */
        }
        if (strlen(m_data) + strlen(time) + 4 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, time);
        strcat(m_data, "'/>");
        m_isInBreak = 1; /* set flag to indicate any text between open and close tag is to be discarded */
    }
    else if (strcmp(element, "prosody") == 0) /* only pitch, rate and volume attributes are supported */
    {
        for (int i = 0; attributes[i]; i += 2)
        {
            if (strcmp(attributes[i], "pitch") == 0)
            {
                char* svoxpitch = convertToSvoxPitch(attributes[i+1]);
                if (!svoxpitch)
                {
                    ALOGE("Error: failed to allocate memory for string!\n");
                    return;
                }
                if (!svoxpitch)
                {
                    svoxpitch = new char[4];
                    if (!svoxpitch)
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                    strcpy(svoxpitch, "100");
                }
                char* pitch = new char[17 + strlen(svoxpitch)];
                if (!pitch)
                {
                    ALOGE("Error: failed to allocate memory for string!\n");
                    return;
                }
                sprintf(pitch, "<pitch level='%s'>", svoxpitch);
                if (strlen(m_data) + strlen(pitch) + 1 > (size_t)m_datasize)
                {
                    if (!growDataSize(100))
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                }
                strcat(m_data, pitch);
                if (!m_appendix)
                {
                    m_appendix = new char[30];
                    m_appendix[0] = '\0';
                }
                strcat(m_appendix, "</pitch>");
                delete [] svoxpitch;
                delete [] pitch;
            }
            else if (strcmp(attributes[i], "rate") == 0)
            {
                char* svoxrate = convertToSvoxRate(attributes[i+1]);
                if (!svoxrate)
                {
                    svoxrate = new char[4];
                    if (!svoxrate)
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                    strcpy(svoxrate, "100");
                }
                char* rate = new char[17 + strlen(svoxrate)];
                if (!rate)
                {
                    ALOGE("Error: failed to allocate memory for string!\n");
                    return;
                }
                sprintf(rate, "<speed level='%s'>", svoxrate);
                if (strlen(m_data) + strlen(rate) + 1 > (size_t)m_datasize)
                {
                    if (!growDataSize(100))
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                }
                strcat(m_data, rate);
                if (!m_appendix)
                {
                    m_appendix = new char[30];
                    if (!m_appendix)
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                    m_appendix[0] = '\0';
                }
                strcat(m_appendix, "</speed>");
                delete [] svoxrate;
                delete [] rate;
            }
            else if (strcmp(attributes[i], "volume") == 0)
            {
                char* svoxvol = convertToSvoxVolume(attributes[i+1]);
                if (!svoxvol)
                {
                    svoxvol = new char[4];
                    if (!svoxvol)
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                    strcpy(svoxvol, "100");
                }
                char* volume = new char[18 + strlen(svoxvol)];
                if (!volume)
                {
                    ALOGE("Error: failed to allocate memory for string!\n");
                    return;
                }
                sprintf(volume, "<volume level='%s'>", svoxvol);
                if (strlen(m_data) + strlen(volume) + 1 > (size_t)m_datasize)
                {
                    if (!growDataSize(100))
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                }
                strcat(m_data, volume);
                if (!m_appendix)
                {
                    m_appendix = new char[30];
                    m_appendix[0] = '\0';
                }
                strcat(m_appendix, "</volume>");
                delete [] svoxvol;
                delete [] volume;
            }
        }
    }
    else if (strcmp(element, "audio") == 0) /* only 16kHz 16bit wav files are supported as src */
    {
        if (strlen(m_data) + 17 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "<usesig file='");

        for (int i = 0; attributes[i]; i += 2)
        {
            if (strcmp(attributes[i], "src") == 0)
            {
                if (strlen(m_data) + strlen(attributes[i+1]) + 1 > (size_t)m_datasize)
                {
                    if (!growDataSize(100))
                    {
                        ALOGE("Error: failed to allocate memory for string!\n");
                        return;
                    }
                }
                strcat(m_data, attributes[i+1]);
            }
        }
        strcat(m_data, "'>");
    }
}

void SvoxSsmlParser::endtagHandler(void* data, const XML_Char* element)
{
    ((SvoxSsmlParser*)data)->endElement(element);
}

void SvoxSsmlParser::endElement(const XML_Char* element)
{
    if (strcmp(element, "speak") == 0)
    {
      /* do nothing */
    }
    else if (strcmp(element, "p") == 0)
    {
        if (strlen(m_data) + 5 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "</p>");
    }
    else if (strcmp(element, "s") == 0)
    {
        if (strlen(m_data) + 5 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "</s>");
    }
    else if (strcmp(element, "phoneme") == 0)
    {
        m_isInBreak = 0; /* indicate we are no longer in phoneme tag */
    }
    else if (strcmp(element, "break") == 0)
    {
        m_isInBreak = 0; /* indicate we are no longer in break tag */
    }
    else if (strcmp(element, "prosody") == 0)
    {
        if (m_appendix)
        {
            if (strlen(m_data) + strlen(m_appendix) + 1 > (size_t)m_datasize)
            {
                if (!growDataSize(100))
                {
                    ALOGE("Error: failed to allocate memory for string!\n");
                    return;
                }
            }
            strcat(m_data, m_appendix);
            delete [] m_appendix;
            m_appendix = NULL;
        }
    }
    else if (strcmp(element, "audio") == 0)
    {
        if (strlen(m_data) + 10 > (size_t)m_datasize)
        {
            if (!growDataSize(100))
            {
                ALOGE("Error: failed to allocate memory for string!\n");
                return;
            }
        }
        strcat(m_data, "</usesig>");
    }
}

void SvoxSsmlParser::textHandler(void* data, const XML_Char* text, int length)
{
    ((SvoxSsmlParser*)data)->textElement(text, length);
}

void SvoxSsmlParser::textElement(const XML_Char* text, int length)
{
    if (m_isInBreak)
    {
        return; /* handles the case when someone has added text inside the break or phoneme tag - this text is thrown away */
    }

    char* content = new char[length + 1];
    if (!content)
    {
        ALOGE("Error: failed to allocate memory for string!\n");
        return;
    }
    strncpy(content, text, length);
    content[length] = '\0';

    if (strlen(m_data) + strlen(content) + 1 > (size_t)m_datasize)
    {
        if (!growDataSize(100))
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return;
        }
    }
    strcat(m_data, content);
    delete [] content;
}

/**
convertToSvoxPitch
Converts SSML pitch labels to SVOX pitch levels
*/
char* SvoxSsmlParser::convertToSvoxPitch(const char* value)
{
    char* converted = NULL;
    if (strcmp(value, "x-low") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_PITCH_XLOW);
    }
    else if (strcmp(value, "low") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_PITCH_LOW);
    }
    else if (strcmp(value, "medium") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_PITCH_MEDIUM);
    }
    else if (strcmp(value, "default") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_PITCH_MEDIUM);
    }
    else if (strcmp(value, "high") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_PITCH_HIGH);
    }
    else if (strcmp(value, "x-high") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_PITCH_XHIGH);
    }
    return converted;
}

/**
    convertToSvoxRate
    Converts SSML rate labels to SVOX speed levels
*/
char* SvoxSsmlParser::convertToSvoxRate(const char* value)
{
    char* converted = NULL;
    if (strcmp(value, "x-slow") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_RATE_XSLOW);
    }
    else if (strcmp(value, "slow") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_RATE_SLOW);
    }
    else if (strcmp(value, "medium") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_RATE_MEDIUM);
    }
    else if (strcmp(value, "default") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_RATE_MEDIUM);
    }
    else if (strcmp(value, "fast") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_RATE_FAST);
    }
    else if (strcmp(value, "x-fast") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_RATE_XFAST);
    }
    return converted;
}

/**
convertToSvoxVolume
Converts SSML volume labels to SVOX volume levels
*/
char* SvoxSsmlParser::convertToSvoxVolume(const char* value)
{
    char* converted = NULL;
    if (strcmp(value, "silent") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_SILENT);
    }
    else if (strcmp(value, "x-low") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_XLOW);
    }
    else if (strcmp(value, "low") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_LOW);
    }
    else if (strcmp(value, "medium") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_MEDIUM);
    }
    else if (strcmp(value, "default") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_MEDIUM);
    }
    else if (strcmp(value, "loud") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_LOUD);
    }
    else if (strcmp(value, "x-loud") == 0)
    {
        converted = new char[4];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_VOLUME_XLOUD);
    }
    return converted;
}

/**
convertBreakStrengthToTime
Converts SSML break strength labels to SVOX break time
*/
char* SvoxSsmlParser::convertBreakStrengthToTime(const char* value)
{
    char* converted = NULL;
    if (strcmp(value, "none") == 0)
    {
        converted = new char[6];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_BREAK_NONE);
    }
    else if (strcmp(value, "x-weak") == 0)
    {
        converted = new char[6];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_BREAK_XWEAK);
    }
    else if (strcmp(value, "weak") == 0)
    {
        converted = new char[6];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_BREAK_WEAK);
    }
    else if (strcmp(value, "medium") == 0)
    {
        converted = new char[6];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_BREAK_MEDIUM);
    }
    else if (strcmp(value, "strong") == 0)
    {
        converted = new char[6];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_BREAK_STRONG);
    }
    else if (strcmp(value, "x-strong") == 0)
    {
        converted = new char[6];
        if (!converted)
        {
            ALOGE("Error: failed to allocate memory for string!\n");
            return NULL;
        }
        strcpy(converted, SSML_BREAK_XSTRONG);
    }
    return converted;
}

/**
growDataSize
Increases the size of the internal text storage member
*/
int SvoxSsmlParser::growDataSize(int sizeToGrow)
{
    char* tmp = new char[m_datasize];
    if (!tmp)
        return 0;

    strcpy(tmp, m_data);
    delete [] m_data;
    m_data = NULL;
    m_data = new char[m_datasize + sizeToGrow];
    if (!m_data)
    {
        m_data = tmp;
        return 0;
    }
    m_datasize += sizeToGrow;
    strcpy(m_data, tmp);
    delete [] tmp;
    tmp = NULL;
    return 1;
}

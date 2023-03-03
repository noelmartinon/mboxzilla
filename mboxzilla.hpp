/*
    BSD 2-Clause License

    Copyright (c) 2017-2019, Noël Martinon
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
#include <iomanip>      // std::setw
#include <limits>       // numeric_limits<int>::max()
#include <algorithm>    // find_if
#include <functional>   // std::not1
#include <time.h>
#include <zlib.h>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

#include "common.hpp"
#include "mbox_parser.hpp"

#include "json.hpp"
#include "cxxopts.hpp"
#include "SimpleIni.h"

//#define ELPP_NO_DEFAULT_LOG_FILE -> specified on command line with gcc -D option
#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP

using json = nlohmann::json;
using namespace std;

json json_remotelist;
std::vector<unsigned char> aes_iv_token;
std::string sToken, ciphertext_token;
string ciphertext;
int nbUploadSuccess = 0, nbUploadError = 0;
string aes_key;
string host_url; // eg: "https://www.domain.net/backup";
int maxlogfiles = 5;
int timeout = 600;
long long speedlimit = 0;

//---------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
/**
 *  sha256()
 *  Generate a 256 SHA hash from string
 */
std::string sha256(string inputstr) {

    unsigned char hash[SHA256_DIGEST_LENGTH];
    std::stringstream stream;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, inputstr.c_str(), inputstr.length());
    SHA256_Final(hash, &sha256);
    int i = 0;
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hash[i]);
    }
    return stream.str();
}
//---------------------------------------------------------------------------------------------
/**
 *  AES_NormalizeKey()
 *  Make a key of exactly 32 bytes using a hash SHA-256
 */
std::string AES_NormalizeKey(std::string inputstr) {

    std::string retVal;
    retVal = sha256(inputstr);
    retVal.resize(32);
    return retVal;
}
//---------------------------------------------------------------------------------------------
/**
 *  url_encode()
 *  Encode a string to be used in a query part of a URL
 */
string url_encode(const std::string &value) {

    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << uppercase;
        escaped << '%' << setw(2) << int((unsigned char) c);
        escaped << nouppercase;
    }

    return escaped.str();
}
//---------------------------------------------------------------------------------------------
/**
 *  AES_Encrypt()
 *  Encrypt vector of char arrays to a vector of char arrays using AES_256_CBC encryption mode
 */
bool AES_Encrypt(string key, std::vector<unsigned char>& iv, std::vector<char>& ptext, std::string& ctext) {

    try {
        if (key.length()!=32)
            throw std::runtime_error("AES-256-CBC key must be 256 bits");

        // Create initialization vector
        std::vector<unsigned char> randbytes(16);
        RAND_bytes(&randbytes[0], 16);
        iv = randbytes;

        EVP_CIPHER_CTX *ctx;
        if(!(ctx = EVP_CIPHER_CTX_new())) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
        int rc = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)key.c_str(), reinterpret_cast<unsigned char*> (&iv[0]));
        if (rc != 1)
            throw std::runtime_error("EVP_EncryptInit_ex failed");

        // Recovered text expands upto BLOCK_SIZE
        ctext.resize(ptext.size()+AES_BLOCK_SIZE);
        int out_len1 = (int)ctext.size();

        rc = EVP_EncryptUpdate(ctx, (unsigned char*)&ctext[0], &out_len1, (const unsigned char*)&ptext[0], (int)ptext.size());
        if (rc != 1)
            throw std::runtime_error("EVP_EncryptUpdate failed");

        int out_len2 = (int)ctext.size() - out_len1;
        rc = EVP_EncryptFinal_ex(ctx, (unsigned char*)&ctext[0]+out_len1, &out_len2);
        if (rc != 1)
            throw std::runtime_error("EVP_EncryptFinal_ex failed");

        // Set cipher text size now that we know it
        ctext.resize(out_len1 + out_len2);
        return true;
    }
    catch (std::runtime_error &e) {
        throw std::runtime_error(e.what ());
        return false;
    }
}
//---------------------------------------------------------------------------------------------
/**
 *  AES_Encrypt()
 *  Encrypt vector of char arrays to a vector of char arrays using AES_256_CBC encryption mode
 */
bool AES_Encrypt(string key, std::vector<unsigned char>& iv, std::vector<char>& ptext, std::vector<char>& ctext) {

    try {
        if (key.length()!=32)
            throw std::runtime_error("AES-256-CBC key must be 256 bits");

        // Create initialization vector
        std::vector<unsigned char> randbytes(16);
        RAND_bytes(&randbytes[0], 16);
        iv = randbytes;

        EVP_CIPHER_CTX *ctx;
        if(!(ctx = EVP_CIPHER_CTX_new())) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
        int rc = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)key.c_str(), reinterpret_cast<unsigned char*> (&iv[0]));
        if (rc != 1)
            throw std::runtime_error("EVP_EncryptInit_ex failed");

        // Recovered text expands upto BLOCK_SIZE
        ctext.resize(ptext.size()+AES_BLOCK_SIZE);
        int out_len1 = (int)ctext.size();

        rc = EVP_EncryptUpdate(ctx, (unsigned char*)&ctext[0], &out_len1, (const unsigned char*)&ptext[0], (int)ptext.size());
        if (rc != 1)
            throw std::runtime_error("EVP_EncryptUpdate failed");

        int out_len2 = (int)ctext.size() - out_len1;
        rc = EVP_EncryptFinal_ex(ctx, (unsigned char*)&ctext[0]+out_len1, &out_len2);
        if (rc != 1)
            throw std::runtime_error("EVP_EncryptFinal_ex failed");

        // Set cipher text size now that we know it
        ctext.resize(out_len1 + out_len2);
        return true;
    }
    catch (std::runtime_error &e) {
        throw std::runtime_error(e.what ());
        return false;
    }
}
//---------------------------------------------------------------------------------------------
/**
 *  AES_Decrypt()
 *  Decrypt string to a STL string using AES_256_CBC encryption mode
 */
void AES_Decrypt(string key, std::vector<unsigned char>& iv, const std::string& ctext, std::string& rtext) {

    EVP_CIPHER_CTX *ctx;
    if(!(ctx = EVP_CIPHER_CTX_new())) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    int rc = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)key.c_str(), reinterpret_cast<unsigned char*> (&iv[0]));
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptInit_ex failed");

    // Recovered text contracts upto BLOCK_SIZE
    rtext.resize(ctext.size());
    int out_len1 = (int)rtext.size();

    rc = EVP_DecryptUpdate(ctx, (unsigned char*)&rtext[0], &out_len1, (const unsigned char*)&ctext[0], (int)ctext.size());
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptUpdate failed");

    int out_len2 = (int)rtext.size() - out_len1;
    rc = EVP_DecryptFinal_ex(ctx, (unsigned char*)&rtext[0]+out_len1, &out_len2);
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptFinal_ex failed");

    // Set recovered text size now that we know it
    rtext.resize(out_len1 + out_len2);
}
//---------------------------------------------------------------------------------------------
/**
 *  AES_Decrypt()
 *  Decrypt string to a vector of char arrays using AES_256_CBC encryption mode
 */
void AES_Decrypt(string key, std::vector<unsigned char>& iv, std::string& ctext, std::vector<char>& rtext) {

    EVP_CIPHER_CTX *ctx;
    if(!(ctx = EVP_CIPHER_CTX_new())) throw std::runtime_error("EVP_CIPHER_CTX_new failed");
    int rc = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)key.c_str(), reinterpret_cast<unsigned char*> (&iv[0]));
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptInit_ex failed");

    // Recovered text contracts upto BLOCK_SIZE
    rtext.resize(ctext.size());
    int out_len1 = (int)rtext.size();

    rc = EVP_DecryptUpdate(ctx, (unsigned char*)&rtext[0], &out_len1, (const unsigned char*)&ctext[0], (int)ctext.size());
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptUpdate failed");

    int out_len2 = (int)rtext.size() - out_len1;
    rc = EVP_DecryptFinal_ex(ctx, (unsigned char*)&rtext[0]+out_len1, &out_len2);
    if (rc != 1)
      throw std::runtime_error("EVP_DecryptFinal_ex failed");

    // Set recovered text size now that we know it
    rtext.resize(out_len1 + out_len2);
}
//---------------------------------------------------------------------------------------------
/**
 *  base64Encode()
 *  Encodes the given data with base64
 */
std::string base64Encode(const std::string &data, size_t len=-1) {

    BIO *bmem, *b64, *bcontainer;
    BUF_MEM *bptr;
    std::string encoded_data;
    if (len == (size_t)-1) len = data.size();

    b64 = BIO_new(BIO_f_base64());
    // we don't want newlines
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    bcontainer = BIO_push(b64, bmem);
    BIO_write(bcontainer, (void*)data.c_str(), len);
    (void) BIO_flush(bcontainer);
    BIO_get_mem_ptr(bcontainer, &bptr);

    char *buf = new char[bptr->length];
    memcpy(buf, bptr->data, bptr->length);
    encoded_data.assign(buf, bptr->length);

    delete [] buf;
    BIO_free_all(bcontainer);

    return encoded_data;
}
//---------------------------------------------------------------------------------------------
/**
 *  base64Decode()
 *  Decodes data encoded with MIME base64
 */
std::string base64Decode(const std::string &data) {

    BIO *bmem, *b64, *bcontainer;

    char *buf = new char[data.size()];
    int decoded_len = 0;
    std::string decoded_data;

    // create a memory buffer containing base64 encoded data
    bmem = BIO_new_mem_buf((void*)data.c_str(), data.size());

    //create a base64 filter
    b64 = BIO_new(BIO_f_base64());

    // we don't want newlines
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // push a Base64 filter so that reading from buffer decodes it
    bcontainer = BIO_push(b64, bmem);

    decoded_len = BIO_read(bcontainer, (void*)buf, data.size());
    BIO_free_all(bcontainer);

    decoded_data.assign(buf, decoded_len);
    delete [] buf;

    return decoded_data;
}
//---------------------------------------------------------------------------------------------
/**
 *  Parse_remote_log()
 *  Parse the response of remote server to logging process
 */
void Parse_remote_log(string logmsg) {

    if (logmsg.empty()) return;
    if (logmsg.find("INFO#") == 0){ LOG(INFO) << logmsg.substr(5); }
    else if (logmsg.find("WARNING#") == 0){ LOG(WARNING) << logmsg.substr(8); }
    else if (logmsg.find("ERROR#") == 0){ throw std::runtime_error(logmsg.substr(6)); }
    else if (logmsg.find("VERBOSE1#") == 0){ VLOG(1) << logmsg.substr(9); }
    else if (logmsg.find("VERBOSE2#") == 0){ VLOG(2) << logmsg.substr(9); }
    else if (logmsg.find("VERBOSE3#") == 0){ VLOG(3) << logmsg.substr(9); }
    //else LOG(INFO) << logmsg;
}
//---------------------------------------------------------------------------------------------
/**
 *  WriteCallback_toBuffer()
 *  CURL callback for writing received data to buffer defined in CURLOPT_WRITEDATA option
 */
size_t WriteCallback_toBuffer(void *buffer, size_t size, size_t nmemb, void *userp) {

    ((std::string*)userp)->append((char*)buffer, size * nmemb);
    return size * nmemb;
}
//---------------------------------------------------------------------------------------------
/**
 *  WriteCallback()
 *  CURL callback for logging
 */
size_t WriteCallback(void *buffer, size_t size, size_t nmemb, void *userp) {

    std::vector<std::string> vSplit;
    string sBuf((char*)buffer);
    sBuf.resize(size*nmemb);

    regex e("[\r\n]+|[^\r\n]+");
    //[.,;-]|[^.,;-]
    sregex_iterator rit(sBuf.begin(), sBuf.end(), e), rend;

    while(rit != rend)
    {
        if (!(rit->str()).empty() && rit->str() !="\r" && rit->str() !="\n") {
            vSplit.push_back(rit->str().c_str());
            Parse_remote_log(rit->str());
        }
        ++rit;
    }

    if (vSplit.size()==0){
        Parse_remote_log(sBuf);
    }

    return size * nmemb;
}
//---------------------------------------------------------------------------------------------
/**
 *  Remote_IsAvailable()
 *  Verify that remote host accept requests to upload eml files
 */
bool Remote_IsAvailable() {

    CURL *curl;
    CURLcode res;
    bool ret = false;
    std::string readBuffer;

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    static const char buf[] =  "Expect:";

    // initialize token
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream token;

    token << std::put_time(&tm, "%Y%m%d_%H%M%S");
    sToken = token.str();

    std::vector<char> vToken(sToken.begin(), sToken.end());
    if (!AES_Encrypt(aes_key, aes_iv_token, vToken, ciphertext_token))
        return false;

    std::string aes_iv_token_str = base64Encode(std::string(aes_iv_token.begin(), aes_iv_token.end()),16);
    string ciphertext_token_b64 = base64Encode(ciphertext_token, ciphertext_token.size());

    curl_global_init(CURL_GLOBAL_ALL);

    /* Now specify the POST data */
    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "token",
             CURLFORM_COPYCONTENTS, ciphertext_token_b64.c_str(),
             CURLFORM_END);

    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "token_iv",
             CURLFORM_COPYCONTENTS, aes_iv_token_str.c_str(),
             CURLFORM_END);

    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "check",
             CURLFORM_COPYCONTENTS, "HELLO",
             CURLFORM_END);

    curl = curl_easy_init();

    // initialize custom header list (stating that Expect: 100-continue is not wanted
    headerlist = curl_slist_append(headerlist, buf);
    if (curl) {

        curl_easy_setopt(curl, CURLOPT_URL, host_url.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback_toBuffer); // Disable standard output
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK && readBuffer == "READY")
            {
                     ret = true;
            }
            else
            {
                     ret = false;
            }
        }

        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all(headerlist);
    }

    if (!curl) throw std::runtime_error("curl_easy_init() failed\n");
    if (res!=0) throw std::runtime_error(curl_easy_strerror(res));

    return ret;
}
//---------------------------------------------------------------------------------------------
/**
 *  Remote_GetList()
 *  Get files stored in remote directory. Use to find out if a file must be uploaded.
 */
bool Remote_GetList(json &j, const std::string outputdir) {

    CURL *curl;
    CURLcode res;
    bool ret = false;
    j.clear();
    std::string readBuffer;

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    static const char buf[] =  "Expect:";

    // initialize token
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream token;
    token << std::put_time(&tm, "%Y%m%d_%H%M%S");
    sToken = token.str();

    std::vector<char> vToken(sToken.begin(), sToken.end());
    if (!AES_Encrypt(aes_key, aes_iv_token, vToken, ciphertext_token))
        return false;

    std::string aes_iv_token_str = base64Encode(std::string(aes_iv_token.begin(), aes_iv_token.end()),16);
    string ciphertext_token_b64 = base64Encode(ciphertext_token, ciphertext_token.size());

    curl_global_init(CURL_GLOBAL_ALL);

    /* Now specify the POST data */
    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "token",
        CURLFORM_COPYCONTENTS, ciphertext_token_b64.c_str(),
        CURLFORM_END);

    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "token_iv",
        CURLFORM_COPYCONTENTS, aes_iv_token_str.c_str(),
        CURLFORM_END);

    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "get_filelist",
        CURLFORM_COPYCONTENTS, outputdir.c_str(),
        CURLFORM_END);

    curl = curl_easy_init();

    // initialize custom header list (stating that Expect: 100-continue is not wanted
    headerlist = curl_slist_append(headerlist, buf);
    if (curl) {

        curl_easy_setopt(curl, CURLOPT_URL, host_url.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback_toBuffer); // Disable standard output
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK)
            {
                j = json::parse(decompress_gzip(readBuffer));
                ret = true;
            }
            else
            {
                ret = false;
            }
        }

        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all(headerlist);
    }

    if (!curl) throw std::runtime_error("curl_easy_init() failed\n");
    if (res!=0) throw std::runtime_error(curl_easy_strerror(res));

    return ret;
}
//---------------------------------------------------------------------------------------------
/**
 *  Remote_SendEml()
 *  Send eml or eml.gz to remote host
 */
bool Remote_SendEml(string fname, std::vector<char> eml) {

    CURL *curl;
    CURLcode res;
    double speed_upload, total_time;
    bool ret = false;

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    static const char buf[] =  "Expect:";

    // initialize ciphertext (eml) and  iv
    std::vector<unsigned char> aes_iv;
    if (!AES_Encrypt(aes_key, aes_iv, eml, ciphertext))
        return false;
    std::string aes_iv_str = base64Encode(std::string(aes_iv.begin(), aes_iv.end()));

    // initialize token
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream token;
    token << std::put_time(&tm, "%Y%m%d_%H%M%S");
    std::string sToken = token.str();

    std::vector<char> vToken(sToken.begin(), sToken.end());
    if (!AES_Encrypt(aes_key, aes_iv_token, vToken, ciphertext_token))
        return false;

    VLOG(3) << "Uploading to " << fname << " (" << bytes_convert(eml.size()) << ")";

    std::string aes_iv_token_str = base64Encode(std::string(aes_iv_token.begin(), aes_iv_token.end()));
    string ciphertext_token_b64 = base64Encode(ciphertext_token, ciphertext_token.size());

    curl_global_init(CURL_GLOBAL_ALL);

    /* Now specify the POST data */
    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "token",
             CURLFORM_COPYCONTENTS, ciphertext_token_b64.c_str(),
             CURLFORM_END);

    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "token_iv",
             CURLFORM_COPYCONTENTS, aes_iv_token_str.c_str(),
             CURLFORM_END);

    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "iv",
             CURLFORM_COPYCONTENTS, aes_iv_str.c_str(),
             CURLFORM_END);

    // set up the header
    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "cache-control:",
        CURLFORM_COPYCONTENTS, "no-cache",
        CURLFORM_END);

    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "content-type:",
        CURLFORM_COPYCONTENTS, "multipart/form-data",
        CURLFORM_END);

    fname = base64Encode(fname); // b64 encoded because COPYNAME strip slash

    curl_formadd(&formpost, &lastptr,
        CURLFORM_COPYNAME, "fileToUpload",  // <--- the (in this case) wanted file-Tag!
        CURLFORM_BUFFER, fname.c_str(),
        CURLFORM_BUFFERPTR, ciphertext.data(),
        CURLFORM_BUFFERLENGTH, ciphertext.size(),
        CURLFORM_END);

    curl = curl_easy_init();

    // initialize custom header list (stating that Expect: 100-continue is not wanted
    headerlist = curl_slist_append(headerlist, buf);
    if (curl) {

        curl_easy_setopt(curl, CURLOPT_URL, host_url.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Disable standard output
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, speedlimit);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res == CURLE_OK) {
            /* now extract transfer info */
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
            VLOG(3) << "Speed was " << bytes_convert(speed_upload) << "/s during " << floor(total_time*100)/100 << " seconds";

            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK)
            {
                //Succeeded
                ret = true;
            }
            else
            {
                //Failed
                ret = false;
            }
        }

        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all(headerlist);
    }

    if (!curl) throw std::runtime_error("curl_easy_init() failed\n");
    if (res!=0) throw std::runtime_error(curl_easy_strerror(res));
    return ret;
}
//---------------------------------------------------------------------------------------------
/**
 *  Remote_SendSyncList()
 *  Send list to server in order to keep only emails or directories listed
 */
bool Remote_SendSyncList(const std::string ListName, std::string SyncDir, std::vector<std::string> vList) {

    json j_sync(vList);
    string sSync = j_sync.dump();
    sSync = compress_gzip(sSync);
    sSync = base64Encode(sSync,sSync.length());

    CURL *curl;
    CURLcode res;
    bool ret = false;

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    static const char buf[] =  "Expect:";

    // initialize token
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream token;
    token << std::put_time(&tm, "%Y%m%d_%H%M%S");
    std::string sToken = token.str();

    std::vector<char> vToken(sToken.begin(), sToken.end());
    if (!AES_Encrypt(aes_key, aes_iv_token, vToken, ciphertext_token))
        return false;

    std::string aes_iv_token_str = base64Encode(std::string(aes_iv_token.begin(), aes_iv_token.end()));
    string ciphertext_token_b64 = base64Encode(ciphertext_token, ciphertext_token.size());

    curl_global_init(CURL_GLOBAL_ALL);

    /* Now specify the POST data */
    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "token",
             CURLFORM_COPYCONTENTS, ciphertext_token_b64.c_str(),
             CURLFORM_END);

    curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "token_iv",
             CURLFORM_COPYCONTENTS, aes_iv_token_str.c_str(),
             CURLFORM_END);

    // set up the header
    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "cache-control:",
        CURLFORM_COPYCONTENTS, "no-cache",
        CURLFORM_END);

    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "content-type:",
        CURLFORM_COPYCONTENTS, "application/json",
        CURLFORM_END);


    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "content-type:",
        CURLFORM_COPYCONTENTS, "multipart/form-data",
        CURLFORM_END);

    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, ListName.c_str(),
        CURLFORM_COPYCONTENTS, sSync.c_str(),
        CURLFORM_END);

    curl_formadd(&formpost,
        &lastptr,
        CURLFORM_COPYNAME, "sync_directory",
        CURLFORM_COPYCONTENTS, SyncDir.c_str(),
        CURLFORM_END);

    curl = curl_easy_init();

    // initialize custom header list (stating that Expect: 100-continue is not wanted
    headerlist = curl_slist_append(headerlist, buf);
    if (curl) {

        curl_easy_setopt(curl, CURLOPT_URL, host_url.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Disable standard output
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK)
            {
                ret = true;
            }
            else
            {
                ret = false;
            }
        }

        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all(headerlist);
    }

    if (!curl) throw std::runtime_error("curl_easy_init() failed\n");
    if (res!=0) throw std::runtime_error(curl_easy_strerror(res));
    return ret;
}
//---------------------------------------------------------------------------------------------
/**
 *  callbackEMLvalid()
 *  Callback function to start callbackEML() when current file is not
 *  in the remote directory content
 */
bool callbackEMLvalid(string dirname, string filename) {

    if (std::find(std::begin(json_remotelist), std::end(json_remotelist),
        filename) == std::end(json_remotelist))
        return true;
    return false;
}
//---------------------------------------------------------------------------------------------
/**
** callbackEML()
** Callback function to start Remote_SendEml()
*/
void callbackEML(string dirname, string filename, std::vector<char> eml) {

    string fullpathfile = dirname + filename;
    if (Remote_SendEml(fullpathfile, eml)) nbUploadSuccess++;
    else nbUploadError++;
}
//---------------------------------------------------------------------------------------------
/**
** callbackLOG()
** Callback function for logging
*/
void callbackLOG(string logtype, string logmsg) {

    if (logtype=="INFO") {LOG(INFO) << logmsg.data();}
    else if (logtype=="ERROR") {LOG(ERROR) << logmsg.data();}
    else if (logtype=="WARNING") {LOG(WARNING) << logmsg.data();}
    else if (logtype=="VERBOSE1") {VLOG(1) << logmsg.data();}
    else if (logtype=="VERBOSE2") {VLOG(2) << logmsg.data();}
    else if (logtype=="VERBOSE3") {VLOG(3) << logmsg.data();}
}
//---------------------------------------------------------------------------------------------
/**
** rolloutHandler()
** Callback function used to rotate log files
*/
void rolloutHandler(const char* filename, std::size_t size) {

    std::stringstream ssSrc, ssDst;
    string fileName = filename;

    size_t pos = fileName.find(".");
    string extractName = (string::npos == pos)? fileName : fileName.substr(0, pos);

    for (int i = maxlogfiles-1; i>=1; i--) {
        ssSrc << extractName << ".log." << i-1;
        ssDst << extractName << ".log." << i;
        if (FileExists(ssSrc.str())) {
            if (FileExists(ssDst.str())) std::remove( ssDst.str().c_str() );
            rename(ssSrc.str().c_str(), ssDst.str().c_str());
        }

        ssSrc.str(std::string()); ssSrc.clear();
        ssDst.str(std::string()); ssDst.clear();

    }

    if (maxlogfiles>1) {
        ssDst << extractName << ".log.1";
        rename(filename, ssDst.str().c_str());
    }
}

//---------------------------------------------------------------------------------------------
/**
 *  IsMboxFile()
 *  Used to list thunderbird mbox
 *  Returns true if source file is mbox type
 */
bool IsMboxFile(std::string filename) {

    char buffer[6] = "0";
    std::ifstream mboxfile( filename, std::ios::binary | std::ios::ate);
    if (mboxfile.fail() || mboxfile.tellg()<5) return false;
    mboxfile.seekg (0, mboxfile.beg);
    mboxfile.read(buffer, 5);
    mboxfile.close();
    if (!strcmp(buffer, "From ")) return true;
    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  GetRegexVector()
 *  Search a regex into string list and extract substring
 *  Returns index of the line where the element has been found
 *  else return -1 if not found
 */
int GetRegexVector (std::vector<std::string>& vString, const std::string& sRegex, std::vector<std::string>& vResult, int iVectorIndex=0) {

    std::regex rgx(sRegex);
    std::smatch match;
    int index=-1;

    vResult.clear();

    for(auto it:vString) {
        if(++index >= iVectorIndex && std::regex_search(it, match, rgx)  && match.size() > 1) {
            for (auto iter: match) {
                vResult.push_back(iter);
            }
            return index;
        }
    }
    return -1;
}
//---------------------------------------------------------------------------------------------
/**
 *  ListThunderbirdMbox()
 *  List the mbox files found in all subfolders of a specified directory
 */
void ListThunderbirdMbox(std::map<std::string, std::vector<std::string>>& map, std::string destination_path, std::string directory, vector<string>& vMboxExcluded) {

    std::vector<string> vListDirectories;

    vListDirectories.push_back(directory);
    ListAllSubDirectories(vListDirectories, directory);

    for (auto itDir:vListDirectories) {
        std::vector<string> vList;
        ListDirectoryContents(vList, itDir, true, false);

        for (auto itFile:vList) {
            string mbx = itDir+"/"+itFile;
            if (IsMboxFile(mbx)) {
                // Extract mbox name only (without account dir) and test if excluded
                mbx = mbx.data()+directory.length()+1;

                bool bIgnore = false;
                if (vMboxExcluded.size()) {
                    for (auto it:vMboxExcluded) {
                        std::regex rgx(it, regex_constants::icase);
                        if (std::regex_match(mbx, rgx)) {
                            LOG(WARNING) << "-> Ignore Mbox file  \""+itDir+"/"+itFile+"\"";
                            bIgnore = true;
                            break;
                        }
                    }
                }

                if (!bIgnore)
                    map[destination_path+"|"+directory].push_back(itDir+"/"+itFile);
            }
        }
    }

    sort(map[destination_path+"|"+directory].begin(),map[destination_path+"|"+directory].end(),NoCaseLess);

}
//---------------------------------------------------------------------------------------------
/**
 *  GetThunderbirdMbox()
 *  Automaticaly retrieve all mbox files of Mozilla Thunderbird email program
 *  for the current user or username if specified.
 *  The optional "email_domain" argument is used to filter email accounts that are found.
 *  Mbox files can be excluded by setting a regex list separated by comma.
 *  Returns the list of mbox files
 */
std::map<std::string, std::vector<std::string>> GetThunderbirdMbox(bool b_getlocalfolders=false, std::string username="", std::string email_domain="", std::string source_exclude="") {

    std::string userpath, tbpath, user="";

    // Search profile path
    #if defined(__linux__) || defined(__APPLE__)
        userpath = path_dusting(getenv("HOME"));
    #elif _WIN32
        userpath = path_dusting(getenv("USERPROFILE"));
    #endif

    if (!DirectoryExists(userpath))
        return std::map<std::string, std::vector<std::string>>();

    // Get the user name and the global users directory path
    size_t pos = userpath.find_last_of("/");
    if (pos != string::npos) {
        if (!username.empty())
            userpath = userpath.substr(0, pos+1) + username;
        user = userpath.substr(pos+1);
    }
    else return std::map<std::string, std::vector<std::string>>();

    // Search thunderbird path
    #if defined(__linux__) || defined(__APPLE__)
        tbpath = userpath+"/.thunderbird";
    #elif _WIN32
        // Path for Windows >= 7
        tbpath = userpath+"/AppData/Roaming/Thunderbird";
        if (!DirectoryExists(tbpath)) {
            // Path for Windows < 7
            tbpath = userpath+"/Application Data/Thunderbird";
        }
    #endif

    if (!DirectoryExists(tbpath)) return std::map<std::string, std::vector<std::string>>();

    // Initialize mbox excluded regex list
    vector<string> vMboxExcluded;
    if (!source_exclude.empty()) {
        split(source_exclude , ',', vMboxExcluded, false);
        // If no comma found then excluded list is the full string
        if (vMboxExcluded.empty())
            vMboxExcluded.push_back(source_exclude);

    }

    // Search thunderbird profiles
    CSimpleIniA ini;
    std::vector<string> vProfiles;
    string profile = tbpath+"/profiles.ini";

    ini.SetUnicode();
    ini.Reset();

    SI_Error rc = ini.LoadFile(profile.c_str());
    if (rc < 0) {
        LOG(ERROR) << "-> No Mozilla Thunderbird profiles.ini file found";
        return std::map<std::string, std::vector<std::string>>();
    }
    else {
        CSimpleIniA::TNamesDepend sections, keys;
        ini.GetAllSections(sections);

        CSimpleIniA::TNamesDepend::const_iterator i;
        for (i = sections.begin(); i != sections.end(); ++i) {
            std::regex rgx("^Profile.*", regex_constants::icase);
            if (std::regex_match(i->pItem, rgx)) {
                const char * pszValue = ini.GetValue(i->pItem, "path", NULL);
                if (pszValue) {
                    vProfiles.push_back(pszValue);
                }
            }
        }
    }

    if (vProfiles.size()==0) {
        LOG(ERROR) << "-> No Mozilla Thunderbird profiles found";
        return std::map<std::string, std::vector<std::string>>();
    }

    // Scan all thunderbird profiles
    std::vector<std::string> lines;
    std::vector<std::string> match;
    std::string line;
    string accountDir;
    string server;
    string email;
    std::map<std::string, std::vector<std::string>> mapFiles;

    for (auto tbprofile:vProfiles) {

        LOG(INFO) << "-> Found profile \""+tbprofile+"\"";
        bool done_localfolders = false; // store current profile local folders always listed
        std::ifstream file(tbpath +"/"+tbprofile+"/prefs.js");

        while ( std::getline(file, line) ) {
            if ( !line.empty() )
                lines.push_back(line);
        }

        // Search all available identities (get 'id#')
        std::map<std::string, std::string> mAccount;
        int idx=-1;
        string regex_filter_base = "\"mail\\.identity\\.(.*)\\.useremail\",.* \"(.*)@(.*)"+email_domain+"\"";
        while ((idx=GetRegexVector(lines, regex_filter_base, match, ++idx))>-1) {

            if (match.size() <= 3) continue;
            email = match[2]+"@"+match[3]+email_domain;

            // Get 'account#'
            // /!\ Sometimes there is many identities and it could be eg "id1,id2"
            string regex_filter = "\"mail\\.account\\.(.*)\\.identities\",.* \"(.*,)?"+match[1]+"(,.*)?\"";
            GetRegexVector(lines, regex_filter, match);
            if (match.size() <= 1) continue;

            // If account always done then continue
            string account = match[1];
            auto iter = mAccount.find( match[1] );
            if( iter != mAccount.cend() ) {
                LOG(WARNING) << "-> Ignore \""+email+"\" account because merged with \""+iter->second+"\"";
                continue;
            }

            // Get 'server#'
            regex_filter = "\"mail\\.account\\."+match[1]+"\\.server\",.* \"(.*)\"";
            GetRegexVector(lines, regex_filter, match);
            if (match.size() <= 1) continue;
            server = match[1];

            // Check if server is not an imap type
            regex_filter= "\"mail\\.server\\."+match[1]+"\\.type\",.* \"(.*)\"";
            GetRegexVector(lines, regex_filter, match);
            if (match.size() <= 1) continue;
            if (match[1] == "imap") {
                LOG(WARNING) << "-> Account \""+email+"\" is ignored because it has an imap type";
                continue;
            }

            // Check value 'deferred_to_account' for current account's 'server#'
            // and if exists then get 'server#' for the corresponding account
            // It often happens when "local folders" is used
            regex_filter= "\"mail\\.server\\."+server+"\\.deferred_to_account\",.* \"(.*)\"";
            GetRegexVector(lines, regex_filter, match);
            if (match.size() > 1) {
                // Get 'server#'
                regex_filter = "\"mail\\.account\\."+match[1]+"\\.server\",.* \"(.*)\"";
                GetRegexVector(lines, regex_filter, match);
                if (match.size() <= 1) continue;
                server = match[1];
            }

            // Get emails directory (in "directory-rel" because "directory" is not always the true path)
            regex_filter = "\"mail\\.server\\."+server+"\\.directory-rel\",.* \"(.*)\"";
            GetRegexVector(lines, regex_filter, match);
            if (match.size() <= 1) continue;

            accountDir = path_dusting(match[1]);
            str_replace(accountDir, "[ProfD]", tbpath +"/"+tbprofile+"/");

            LOG(INFO) << "-> Found account \""+email+"\"";
            mAccount[account]=email;

            // If an account directory is local folders then ignore
            if (accountDir.compare(tbpath +"/"+tbprofile+"/Mail/Local Folders") == 0){
                done_localfolders = true;
                if (b_getlocalfolders)
                    LOG(INFO) << "-> \"Local folders\" is merged with \""+email+"\" account";
            }

            // List current account content
            string tbprofile_only = tbprofile;
            size_t pos = tbprofile.find_last_of("/");
            if (pos != string::npos)
                tbprofile_only = tbprofile.substr(pos+1);
            ListThunderbirdMbox(mapFiles, user+"/"+tbprofile_only+"/"+email, accountDir, vMboxExcluded);

        }

        // List profile local folders content
        if (b_getlocalfolders && !done_localfolders){
            string tbprofile_only = tbprofile;
            size_t pos = tbprofile.find_last_of("/");
            if (pos != string::npos)
                tbprofile_only = tbprofile.substr(pos+1);
            ListThunderbirdMbox(mapFiles, user+"/"+tbprofile_only+"/"+"Local Folders", tbpath +"/"+tbprofile+"/Mail/Local Folders", vMboxExcluded);
            done_localfolders = true;
            LOG(INFO) << "-> \"Local folders\" is processed separately";
        }

    } // End auto vProfiles

    return mapFiles;
}
//---------------------------------------------------------------------------------------------
/**
 *  Remove_EmptyDir()
 *  Remove all empty directories in the specified directory
 */
void Remove_EmptyDir(std::string directory) {

    std::vector<string> vListDirectory;
    ListAllSubDirectories(vListDirectory, directory);

    // Sort and reverse list because we need to verify child directories before parents
    // in order to recursively delete empty dir
    sort(vListDirectory.begin(), vListDirectory.end());
    std::reverse(vListDirectory.begin(), vListDirectory.end());

    for (auto itDir:vListDirectory) {
        std::vector<string> vList;
        ListDirectoryContents(vList, itDir, true, true);
        if (vList.empty())
            std::remove(itDir.c_str());
    }
}
//---------------------------------------------------------------------------------------------


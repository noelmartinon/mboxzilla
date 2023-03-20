/*
    BSD 2-Clause License

    Copyright (c) 2017-2023, Noël Martinon
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

#ifndef __MBOXPARSER_HPP
#define __MBOXPARSER_HPP

#include <iostream>     //setfill
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>       //remove
#include <stdexcept>
#include <algorithm>
#include <cmath>        //ceil
#include <regex>
#include <dirent.h>
#include "nsMsgMessageFlags.h"
#include "simplyzip.hpp"

using namespace std;

class Mbox_parser {

    static const char *Anim[];
    static float iAnim;

    private:

        std::ifstream mboxfile;
        string mboxfilename; // only file name
        string mboxfullname; // path + file name
        bool readytoparse;
        size_t mboxindex; // mbox file reading pointer
        size_t mboxlength; // mbox file length
        int nbmailread; // nb mail's headers read ("From ")
        int nbmailinvalid; // nb invalid or malformed mails
        int nbmaildeleted; // nb deleted mails
        int nbmailduplicated; // nb duplicated emails
        int nbmailok; // nb available mails
        int nbmailexcluded; // valid mails but excluded by filters age, date
        int nbmailextracted; // nb new eml files created
        int nbmailcompact; // nb mails write to compact file
        int nbmailsplit; // nb mails write to split files
        int nbemlremoved; // nb eml (or .gz) files removed when syncing
        int mailAgeMin; // Minimum age in days that the mails must have (eg: older than 30 days)
        int mailAgeMax; // Maximum age in days that the mails must have (eg: younger than 90 days)
        std::string outputdirectory;
        std::string compactfilename;
        std::ofstream outputcompact;
        std::string splitfilename;
        std::ofstream outputsplit;
        size_t i_progression; // mbox process progression percentage
        bool islastmail; // last mail of mbox that doesn't ending with search string "From "
        std::vector<char> buffer; // file read buffer
        std::vector<char> vmails; // mails vector (can content many mails)
        std::vector<char> vmail; // mail vector with "From " line use in compact or split function
        std::vector<char> vheader;
        std::vector<char> vmailcrlf; // store eml (or eml.gz) with windows crlf use in extraction or callback_eml function
        size_t mailsize; // Size begin with "From " to next one
        std::string headerfield_date; // Store "Date:" header field value to avoid multiplying search
        std::string headerfield_from; // Store "From:" header field value to avoid multiplying search
        std::string headerfield_msgid; // Store "Message-ID:" header field value to avoid multiplying search
        vector<string> emlList; // list of valid eml file name
        bool bEmlToWindows;
        bool bSynchronize;
        bool bGenerateMboxCompact;
        bool bDisableMboxCompact; // Avoid to append compact file when there has been a previous 'open or write' error
        bool bExtractMboxEml;
        bool bGenerateMboxSplit;
        bool bDisableMboxSplit; // Avoid to append split file when there has been a previous 'open or write' error
        bool bCompressEml;
        bool bExtractInvalid;
        bool bExtractDeleted;
        bool bExtractDuplicated;
        int splitindex;
        size_t mboxsplitmaxsize;
        size_t mboxsplitcurrentsize;
        int nbsplitfile;
        bool bmaildatestored; // marked to avoid multi call of function GetMailDate()
        std::string emlfilename;
        std::string newline; // Read for each mail of mbox file because mbox can contains mails with "\n" as well as "\r\n"
        int hourlocalTZ; // timezone local
        int minutelocalTZ; // timezone local
        struct tm tm_maildate; //
        time_t tt_timezero; //store time(0) of beginning parse
        time_t tt_maildate;
        time_t tt_maildatebefore;
        time_t tt_maildateafter;

        typedef bool (*callback_func_eml_preprocess_ptr)(std::string, std::string);
        callback_func_eml_preprocess_ptr cbFunc_eml_preprocess;
        typedef void (*callback_func_eml_process_ptr)(std::string, std::string, std::vector<char>); // vector is email's content
        callback_func_eml_process_ptr cbFunc_eml_process;
        typedef void (*callback_func_log_ptr)(std::string, std::string);
        callback_func_log_ptr cbFunc_log;

        void ShowProgressBar();
        bool IsMboxFile();
        void Init();
        void ProcessPacket();
        bool FindMailSeparator(bool bUseAsctime=false);
        void ProcessMail();
        std::string GetHeaderField(std::string headerField, bool insensitiveSearch=false, int index=0);
        void GetLocalTimeZone();
        bool IsValidMail();
        bool IsDeletedMail();
        bool IsExcludedMail();
        std::string EmlFilename(); // Generate eml filename from mail headers
        void StoreEML(); // Set vmailcrlf to save and callback functions
        bool SaveToEML();
        bool SaveToCompact();
        bool SaveToSplit();
        bool GetMailDate(bool is_forcesearch=false);

    public:
        Mbox_parser(std::string const="");
        ~Mbox_parser();
        bool SetMboxFile(std::string const);
        bool IsReadyToParse();
        int Parse();
        int GetMailAvailable();
        int GetMailRead();
        int GetMailInvalid();
        int GetMailDeleted();
        int GetMailDuplicated();
        int GetMailExcluded();
        int GetMailExtracted();
        int GetMailCompact();
        int GetMailSplit();
        int GetSplitFile();
        int GetEmlDeleted();
        std::vector<string> GetEmlList();
        void SetWindowsFormat(bool b);
        void SetSaveEmlList(bool b);
        void SetSynchronize(bool b);
        void SetActionExtract(bool b, bool compress = true);
        void SetActionCompact(bool b);
        void SetActionSplit(bool b, size_t maxsize);
        std::string SetOutputDirectory(std::string directory);
        void SetExtractInvalid(bool bExtract);
        void SetExtractDeleted(bool bExtract);
        void SetExtractDuplicated(bool bExtract);
        void SetAgeMin(int age); // minimal mail's age in days (more recent, default now)
        void SetAgeMax(int age); // maximal mail's age in days
        bool SetDateBefore(std::string strdate);
        bool SetDateAfter(std::string strdate);
        void Set_Callback_Eml_Preprocess(callback_func_eml_preprocess_ptr ptr);
        void Set_Callback_Eml_Process(callback_func_eml_process_ptr ptr);
        void Set_Callback_Log(callback_func_log_ptr ptr);
};

#endif /* !__MBOXPARSER_HPP */

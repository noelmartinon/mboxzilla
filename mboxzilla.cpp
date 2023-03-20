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


/****** Application description and notice ******/
#define APP_VERSION "1.3.0"

#define APP_INFO "\
\n\
mboxzilla version " APP_VERSION "\n\
Copyright (C) 2017-2019 Noel Martinon. All rights reserved.\n\
"

#define APP_DESCRIPTION "\
License:\n\
  mboxzilla comes with ABSOLUTELY NO WARRANTY. This is free software, and you\n\
  are welcome to redistribute it under certain conditions. See the BSD\n\
  2-Clause License for details.\n\
  \n\
Features:\n\
  mboxzilla allows to extract emails from mbox files, to compact and to\n\
  split mbox files to a specfified folder. It can also upload eml to a remote\n\
  host and sync. These actions are done according to the optional date filters.\n\
  An auto processing is available to Mozilla Thunderbird client.\n\
"

#define APP_NOTICE "\
Examples and more:\n\
  See the readme file or https://github.com/noelmartinon/mboxzilla\n\
"
/*************************************/

#include "mboxzilla.hpp"

int main(int argc, char**argv)
{
    Mbox_parser mbox;
    std::vector<std::string> vmboxfile;
    std::map<std::string, std::vector<std::string>> mapmbox; // <output subdir, mbox files>
    std::vector<std::string> voutputdir;
    std::string outputdir;
    std::string outputpath;
    std::string logfilename;
    bool bGetLocalFolders=false;
    std::string username="";
    std::string email_domain="";
    std::string source_exclude="";
    bool bAuto = false;
    bool bActionExtract = false;
    bool bActionCompact = false;
    bool bActionSplit = false;
    int iSplitMaxSize = 0;
    bool bEmlCompress = false;
    bool bExtractInvalid = false;
    bool bExtractDeleted = false;
    bool bExtractDuplicated = false;
    bool bSynchonize = false;
    bool bWindowsFormat = false;
    int age_min = 0;
    int age_max = 0;
    string date_before, date_after;
    int start_wait = 0, start_random = 0;

    int total_mbox=0;
    int total_read=0;
    int total_available=0;
    int total_invalid=0;
    int total_deleted=0;
    int total_duplicated=0;
    int total_excluded=0;
    int total_extracted=0;
    int total_emldeleted=0;
    int total_compact_emails=0;
    int total_compact_files=0;
    int total_split_emails=0;
    int total_split_files=0;
    int total_upload_succeed=0;
    int total_upload_failed=0;


    try {
        cxxopts::Options options("mboxzilla", APP_DESCRIPTION, "[CONFIG_FILE]");

        options.add_options()
            ("f,file",
                "Input mbox file.", cxxopts::value<std::vector<std::string>>(vmboxfile), "FILE")
            ("o,output",
                "Output directory.", cxxopts::value<std::string>(outputdir), "DIR")
            ("p,path",
                "Base input path where mbox file is stored. "
                "When is set then input mbox specified by 'f' option is concatenated to output directory. "
                "If PATH is a substring from input mbox file then it is from string following the path argument.",
                cxxopts::value<std::string>(outputpath), "PATH")
            ("e,extract",
                "Extract emails from mbox file to eml files named 'YYYYmmddHHMMSS_MD5.eml' (or eml.gz) "
                "to a local directory.",
                cxxopts::value<bool>(bActionExtract))
            ("c,compact",
                "Compact mbox file to file whose name is formatted in 'mboxfilename_YYYYmmddHHMMSS'.",
                    cxxopts::value<bool>(bActionCompact))
            ("s,split",
                "Split mbox file into several mbox files of maximum N bytes size. "
                "This smaller files are named 'mboxfilename.#' where # is an increment number with auto leading zero if necessary.",
                cxxopts::value<int>(iSplitMaxSize), "N")
            ("a,auto",
                "Automatic mbox files search and parse for Mozilla Thunderbird client. The search is performed "
                "in the current user directory.",
                cxxopts::value<bool>(bAuto))
            ("with-localfolders",
                "Force processing the 'Thunderbird local folders' directory for all profiles selected. Only used with 'auto' option.",
                cxxopts::value<bool>(bGetLocalFolders))
            ("force-user",
                "Set the user name profile for Thunderbird search. Only used with 'auto' option.",
                cxxopts::value<std::string>(username), "USER")
            ("email-domain",
                "Keep only corresponding accounts found in Thunderbird. Only used with 'auto' option.",
                cxxopts::value<std::string>(email_domain), "DOMAIN")
            ("source-exclude",
                "Exclude mbox files from Thunderbird list. This is a insensitive regex list separated by comma. Only used with 'auto' option.",
                cxxopts::value<std::string>(source_exclude), "REGEX")
            ("w,windows-format",
                "Convert extracted eml files to windows format.",
                cxxopts::value<bool>(bWindowsFormat))
            ("synchronize",
                "Synchonize eml files from available emails list and if 'auto' is set then keep only valid Thunderbird directories.",
                cxxopts::value<bool>(bSynchonize))
            ("z,compress",
                "Compress eml in gzip format and add extension '.gz' to file name.",
                cxxopts::value<bool>(bEmlCompress))
            ("i,with-invalid",
                "Invalid emails are retained. This status is defined when at least one of the 'date' "
                "or 'from' fields is missing from the header. "
                "The date filters do not affect these emails. "
                "The names are formatted in '00000000000000_MD5.eml' (or eml.gz).",
                cxxopts::value<bool>(bExtractInvalid))
            ("x,with-duplicated",
                "Duplicated emails are retained during processing.",
                cxxopts::value<bool>(bExtractDuplicated))
            ("d,with-deleted",
                "Deleted emails are retained during processing. This status is linked to the 'X-Mozilla-Status' "
                "header field and therefore only available with Mozilla Thunderbird email client.",
                cxxopts::value<bool>(bExtractDeleted))
            ("u,url",
                "Url for the messages uploading process in eml or gz file format. This option require option 'k' "
                "to be set to trigger the remote sending process. It is independent of 'e' option.",
                cxxopts::value<std::string>(host_url), "URL")
            ("k,key",
                "Password used to secure exchanges with the remote host.",
                    cxxopts::value<string>(), "KEY")
            ("age-min",
                "Select emails that have more than N days.",
                    cxxopts::value<int>(age_min), "N")
            ("age-max",
                "Select emails that have less than N days.",
                    cxxopts::value<int>(age_max), "N")
            ("date-before",
                "Select emails before the specified date. DATE must be formatted in 'YYYY-mm-dd HH:MM:SS' "
                "or 'YYYY/mm/dd HH:MM:SS'. The parameters 'HH:MM:SS' are optional and would be set to "
                "'00:00:00' if it is missing.",
                    cxxopts::value<string>(date_before), "DATE")
            ("date-after",
                "Select emails after the specified date. Same syntax as 'date-before'",
                    cxxopts::value<string>(date_after), "DATE")
            ("timeout",
                "Set maximum time in seconds the remote connection request is allowed to take. Used if 'u' option is set. "
                "WARNING: If defined to 0 then process could hang.",
                    cxxopts::value<int>(timeout)->default_value("600"), "N")
            ("speed-limit",
                "Set maximum speed in bytes per second to send a file. Used if 'u' option is set.",
                    cxxopts::value<long long>(speedlimit)->default_value("0"), "N")
            ("start-wait",
                "Delay process waiting to start in seconds.",
                    cxxopts::value<int>(start_wait)->default_value("0"), "N")
            ("start-random",
                "Maximum delay before process start in seconds. The random value is added to the 'wait' countdown.",
                    cxxopts::value<int>(start_random)->default_value("0"), "N")
            ("log-file",
                "Log what we're doing to the specified FILE.",
                cxxopts::value<std::string>(logfilename), "FILE")
            ("log-maxfiles",
                "Maximum number of log files. Each log file size does not exceed 1 MB.",
                cxxopts::value<int>(maxlogfiles)->default_value("5"), "N" )
            ("v,verbose",
                "Verbose level for eml processing (N between 1 and 3, 3 is implicit). "
                "1=ERROR, 2=WARNING, 3=INFO.",
                cxxopts::value<int>()->implicit_value("3"), "N")
            ("version",
                "Display version number.")
            ("help",
                "Display command line options.")
        ;

        // load settings from a configuration file
        CSimpleIniA ini;
        if (argc==2 && ini.LoadFile(argv[1])>=0) {

            std::vector<string> args;
            CSimpleIniA::TNamesDepend keys;
            ini.GetAllKeys("", keys);

            CSimpleIniA::TNamesDepend::const_iterator i;
            std::string s_argv;
            args.push_back(argv[0]);
            for (i = keys.begin(); i != keys.end(); ++i) {
                // get the value of a key
                const char * pszValue = ini.GetValue("", i->pItem, NULL);
                if (std::string(pszValue).empty()) {
                    std::string msg = u8"Empty value for option '";
                    msg += i->pItem;
                    msg += "'";
                    throw cxxopts::OptionSpecException(msg);
                }

                if (std::string(pszValue)=="false") continue;
                else if (std::string(pszValue)=="true")
                    s_argv = "--" + std::string(i->pItem);
                else s_argv = "--" + std::string(i->pItem) + "=" + std::string(pszValue);

                args.push_back(s_argv.c_str());
            }

            // reset 'argc' and 'argv' (executable name + ini keys + NULL)
            argc = args.size();
            char **argv = new char*[argc+1];
            for (size_t j = 0;  j < argc;  ++j)     // copy args
                argv[j] = (char*)args[j].c_str();
            argv[argc] = NULL;

            options.parse(argc, (char**&)argv);

            // Case bad option in the file
            if (argc>2) {
                std::string msg = u8"Too many or unknown specified options ";
                for (int i=1; i<argc; i++) {
                    msg = msg+"'"+argv[i]+"'";
                    if (i+1<argc) msg += ", ";
                }
                throw cxxopts::OptionSpecException(msg);
            }

        }
        else {
            options.parse(argc, argv);

            if (argc>1) {
                std::string msg = u8"Too many or unknown specified options ";
                for (int i=1; i<argc; i++) {
                    msg = msg+"'"+argv[i]+"'";
                    if (i+1<argc) msg += ", ";
                }
                throw cxxopts::OptionSpecException(msg);
            }
        }

        if (options.count("version"))
        {
          std::cout << APP_VERSION << endl;
          exit(0);
        }

        if (options.count("help"))
        {
          std::cout << APP_INFO << std::endl;
          std::cout << options.help({"", "Group"}) << std::endl;
          std::cout << APP_NOTICE << endl;
          exit(0);
        }

        if (options.count("e") && (options.count("s")||options.count("c")) && bSynchonize)
        {
          throw cxxopts::OptionSpecException(u8"Option 'e' is not compatible with 's' or 'c' when sync is enabled.");
          exit(0);
        }

        if (options.count("v"))
        {
            int value = options["v"].as<int>();
            if (value<1 || value>3)
                throw cxxopts::OptionSpecException(u8"Option 'v' requires an optional value between 1 and 3 (implicitly 3)");
            el::Loggers::setVerboseLevel(value);
        }

        if (!options.count("f") && !options.count("a"))
        {
            throw cxxopts::OptionSpecException(u8"Option 'f' or 'a' is required");
        }

        if (options.count("s"))
        {
            bActionSplit = true;
            if (iSplitMaxSize<=0)
                throw cxxopts::OptionSpecException(u8"Option 's' requires a positive value");
        }

        if ((options.count("u") && !options.count("k")) ||
            (!options.count("u") && options.count("k"))) {
                throw cxxopts::OptionSpecException(u8"Options 'u' and 'k' are linked and must both be configured");
        }

        if (options.count("speed-limit") && !options.count("u") && !options.count("k")) {
                throw cxxopts::OptionSpecException(u8"Option 'speed-limit' can not be used without options 'u' and 'k'");
        }

        if (options.count("u")){
            host_url = options["u"].as<std::string>();
        }

        if (options.count("k")){
            aes_key = AES_NormalizeKey(options["k"].as<std::string>());
        }

        if (options.count("age-min") && options.count("date-before")){
                throw cxxopts::OptionSpecException(u8"Options 'age-min' and 'date-before' can not be specified at the same time");
        }

        if (options.count("age-max") && options.count("date-after")){
                throw cxxopts::OptionSpecException(u8"Options 'age-max' and 'date-after' can not be specified at the same time");
        }

        if (options.count("timeout")){
            if (timeout<0) throw cxxopts::OptionSpecException(u8"Option 'timeout' required a positive value");
        }

        if (options.count("log-file"))
        {
            el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Filename, logfilename);
        }
    }
    catch (const cxxopts::OptionException& e) {
        std::cout << APP_INFO << std::endl;
        std::cout << "Error parsing options: " << e.what() << std::endl;
        std::cout << "Try --help for usage information." << std::endl << std::endl;
        exit(1);
    }

    std::cout << APP_INFO << std::endl;

    el::Configurations defaultConf;

    defaultConf.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, "1048576");
    defaultConf.set(el::Level::Global, el::ConfigurationType::Format, "%datetime %level %msg");

    el::Loggers::reconfigureLogger("default", defaultConf);
    el::Loggers::addFlag(el::LoggingFlag::StrictLogFileSizeCheck);
    el::Loggers::addFlag(el::LoggingFlag::DisableApplicationAbortOnFatalLog); // disable msg "WARN  Aborting application. Reason: Fatal log at [easylogging++.h:5627]"
    el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);

    el::Helpers::installPreRollOutCallback(rolloutHandler);


    if (start_wait || start_random) {
        srand((unsigned)time(0));
        unsigned int wait_time = start_wait + (rand() % (int)(start_random + 1)); // microseconds on linux, milliseconds on Windows
        cout << "Waiting " << wait_time << " seconds before start..." << endl;
        wait_time = wait_time*1000;
        Sleep(wait_time);
    }

    try {

        LOG(INFO) << "STARTING mboxzilla";
        if (speedlimit)
            LOG(INFO) << "Maximum speed to upload files is set to "+std::to_string(speedlimit)+" B/s";

        // Set global mbox options
        mbox.SetActionExtract(bActionExtract, bEmlCompress);
        mbox.SetActionCompact(bActionCompact);
        mbox.SetActionSplit(bActionSplit, iSplitMaxSize);
        mbox.SetExtractInvalid(bExtractInvalid);
        mbox.SetExtractDeleted(bExtractDeleted);
        mbox.SetExtractDuplicated(bExtractDuplicated);
        mbox.SetWindowsFormat(bWindowsFormat);
        mbox.SetSynchronize(bSynchonize);
        mbox.Set_Callback_Log(&callbackLOG);

        // Add mbox files set with 'f' option to mbox list
        mapmbox[""] = vmboxfile;

        // If 'a' option is set (thunderbird search) then search mbox files and add them to mbox list
        if (bAuto) {
            LOG(INFO) << "Searching for Mozilla Thunderbird profiles";

            std::map<std::string, std::vector<std::string>> mapMailMbox = GetThunderbirdMbox(bGetLocalFolders, username, email_domain, source_exclude);

            for(auto const& key : mapMailMbox) {
                mapmbox[key.first] = key.second;
            }

            if (!mapMailMbox.size()) LOG(ERROR) << "No Mozilla Thunderbird mbox files found";
        }

        for(auto const& key : mapmbox) {
            string outdirfinal = outputdir;
            string outputpathfinal = outputpath;

            if (!key.first.empty()){
                vector<string> v;
                split(key.first , '|', v, false);
                if (!outdirfinal.empty() && *outdirfinal.rbegin() != '/')
                    outdirfinal += "/";
                outdirfinal += v[0];
                outputpathfinal = v[1];
            }

            for(auto const& mboxfile : key.second) {

                string outdir = path_dusting(outdirfinal);
                string infile = path_dusting(mboxfile);

                if (outputpathfinal.length()){
                    size_t pos = mboxfile.find(outputpathfinal);
                    if (pos != string::npos)
                        outdir = outdirfinal+"/"+mboxfile.substr(pos+outputpathfinal.length(), mboxfile.length());
                    else {
                        infile = outputpathfinal+"/"+mboxfile;
                        outdir = outdirfinal+"/"+mboxfile;
                    }
                }

                // If thunderbird is processed then rename output subdirectories
                if (!key.first.empty()) {
                    str_replace(outdir, ".sbd/", "/");
                }

                outdir = mbox.SetOutputDirectory(outdir); // ending with '/'

                mbox.SetAgeMin(age_min);
                mbox.SetAgeMax(age_max);

                if (!date_before.empty() && !mbox.SetDateBefore(date_before)){
                    throw std::runtime_error("'date-before' is not formatted properly\n");
                }
                if (!date_after.empty() && !mbox.SetDateAfter(date_after)){
                    throw std::runtime_error("'date-after' is not formatted properly\n");
                }

                LOG(INFO) << "INPUT FILE is \""+infile+"\"";
                LOG(INFO) << "OUTPUT DIRECTORY is "+((outdir.empty())?"undefined":"\""+outdir+"\"");

                if (mbox.SetMboxFile(infile)){

                    bool remote_ok = false;
                    if (!host_url.empty()) {
                        try {
                            remote_ok = Remote_IsAvailable();

                            if (!remote_ok ) {
                                    LOG(ERROR) << "Remote connection to \""+host_url+"\" unavailable";
                                    // Disable EML Callback functions
                                    mbox.Set_Callback_Eml_Preprocess(&callbackEMLvalid);
                                    mbox.Set_Callback_Eml_Process(NULL);
                                    if (!bActionExtract && !bActionCompact && !bActionSplit) continue;
                            }
                            else {
                                LOG(INFO) << "Remote connection to \""+host_url+"\" ready";
                                mbox.Set_Callback_Eml_Preprocess(&callbackEMLvalid);
                                mbox.Set_Callback_Eml_Process(&callbackEML);
                                Remote_GetList(json_remotelist, outdir);
                            }

                            nbUploadSuccess=0; nbUploadError=0;
                        }
                        catch (const std::exception& ex) {
                            LOG(ERROR) << "Connection exception : " << ex.what();
                            remote_ok = false;
                        }
                    }

                    bool bExceptionOccurred = false; // Used to disable files synchronization if partial parsing
                    try {
                        total_mbox++;
                        mbox.Parse();
                    }
                    catch (const std::exception& ex) {
                        LOG(ERROR) << "Parse exception : " << ex.what();
                        bExceptionOccurred = true;
                    }

                    // Clear directories
                    if (bActionExtract || bActionCompact || bActionCompact)
                        Remove_EmptyDir(outdirfinal);

                    // Add directory for sync if Thunderbird is processed and (extract or upload)
                    // and if has emails or parsing is in error to not delete previous exported emails
                    if ((mbox.GetMailAvailable()>0 || bExceptionOccurred) &&
                        !key.first.empty() && (bActionExtract || !host_url.empty()))
                        voutputdir.push_back(outdir);

                    // Next lines are out of 'try' because the parsing process may be partial
                    if (mbox.GetMailAvailable()>=0) {
                        LOG(INFO) << "Summary :";
                        LOG(INFO) << "-> " << mbox.GetMailAvailable() << " available / " << mbox.GetMailRead() << " found";
                        LOG(INFO) << "-> invalid = " << mbox.GetMailInvalid();
                        LOG(INFO) << "-> deleted = " << mbox.GetMailDeleted();
                        LOG(INFO) << "-> duplicated = " << mbox.GetMailDuplicated();
                        LOG(INFO) << "-> excluded = " << mbox.GetMailExcluded();
                        total_available += mbox.GetMailAvailable();
                        total_read += mbox.GetMailRead();
                        total_invalid += mbox.GetMailInvalid();
                        total_deleted += mbox.GetMailDeleted();
                        total_duplicated += mbox.GetMailDuplicated();
                        total_excluded += mbox.GetMailExcluded();

                        if (bActionExtract) {
                            if (bEmlCompress) LOG(INFO) << "-> extracted to eml.gz = " << mbox.GetMailExtracted();
                            else LOG(INFO) << "-> extracted to eml = " << mbox.GetMailExtracted();
                            if (bSynchonize) LOG(INFO) << "-> removed from destination = " << mbox.GetEmlDeleted();
                            total_extracted += mbox.GetMailExtracted();
                            total_emldeleted += mbox.GetEmlDeleted();
                        }

                        if (bActionCompact) {
                            LOG(INFO) << "-> emails in compact file = " << mbox.GetMailCompact();
                            total_compact_emails += mbox.GetMailCompact();
                            total_compact_files += 1;
                        }

                        if (bActionSplit) {
                            LOG(INFO) << "-> emails in split files = " << mbox.GetMailSplit();
                            LOG(INFO) << "-> number of split files = " << mbox.GetSplitFile();
                            total_split_emails += mbox.GetMailSplit();
                            total_split_files += mbox.GetSplitFile();
                        }

                        if (remote_ok) {
                            LOG(INFO) << "-> uploads succeed = " << nbUploadSuccess;
                            LOG(INFO) << "-> uploads failed = " << nbUploadError;
                            total_upload_succeed += nbUploadSuccess;
                            total_upload_failed += nbUploadError;

                            if (bSynchonize && !bExceptionOccurred) {
                                LOG(INFO) << "Syncing files to \""+host_url+"\"";
                                if (Remote_SendSyncList("sync_filelist", outdir, mbox.GetEmlList())) LOG(INFO) << "Synchronization done";
                                else LOG(ERROR) << "Synchronization not completed";
                            }
                        }
                    }
                }
            } // END key.second loop
        } // END mapmbox loop

        // Synchronize directory tree (remove old dir - apply only on Thunderbird)
        if (bSynchonize && voutputdir.size()) {
            string outdirbase = voutputdir[0]; // Retrieve base directory that is outputdir/username
            std::vector<char> data(outdirbase.begin(), outdirbase.end());
            size_t pos = offset(data, "/", outputdir.length()+1);
            outdirbase = outdirbase.substr(0, pos);

            if (!host_url.empty()){
                LOG(INFO) << "Syncing directories to \""+host_url+"\"";

                if (Remote_SendSyncList("sync_dirlist", outdirbase, voutputdir)) LOG(INFO) << "Synchronization done";
                else LOG(ERROR) << "Synchronization not completed";
            }

            if (bActionExtract) {
                std::vector<string> vListDirectories;
                vListDirectories.push_back(outdirbase);
                ListAllSubDirectories(vListDirectories, outdirbase);

                for (auto& dir : vListDirectories)
                    dir = dir+"/";

                vector<string> vListDiff;
                sort(vListDirectories.begin(), vListDirectories.end());
                sort(voutputdir.begin(), voutputdir.end()); // voutputdir ending with '/'

                // Search elements of vListDirectories which are not found in the sorted voutputdir
                set_difference(vListDirectories.begin(),vListDirectories.end(),voutputdir.begin(),voutputdir.end(),back_inserter(vListDiff));

                std::vector<std::string> v_dirtoremove;

                for (auto dir:vListDiff) {
                    bool isParentDir = false;
                    for (auto& str : voutputdir) {
                        if (str.find(dir) == 0) {
                            isParentDir = true;
                            break;
                        }
                    }
                    if (!isParentDir)
                        v_dirtoremove.push_back(dir);
                }

                std::reverse(v_dirtoremove.begin(),v_dirtoremove.end());
                for (auto dir:v_dirtoremove) {
                    std::vector<string> vListFiles;

                    // List files (only) contains in directory output
                    if (ListDirectoryContents(vListFiles, dir, true, false)) {
                        for(string file : vListFiles){
                            file = dir + file;
                            std::remove(file.c_str());
                        }
                        int ret = std::remove(dir.c_str());
                        if (!ret) LOG(INFO) <<  "Directory \""+dir+"\" was deleted";
                        else LOG(ERROR) << "WARNING", "Can not delete directory \""+dir+"\"";
                    }
                }
            }
        }

        // Summary of the results
        if (total_mbox>1 && total_available>=0) {
            LOG(INFO) << "Summary of the " << total_mbox << " mbox files processed :";
            LOG(INFO) << "-> " << total_available << " available / " << total_read << " found";
            LOG(INFO) << "-> invalid = " << total_invalid;
            LOG(INFO) << "-> deleted = " << total_deleted;
            LOG(INFO) << "-> duplicated = " << total_duplicated;
            LOG(INFO) << "-> excluded = " << total_excluded;

            if (bActionExtract) {
                if (bEmlCompress) LOG(INFO) << "-> extracted to eml.gz = " << total_extracted;
                else LOG(INFO) << "-> extracted to eml = " << total_extracted;
                if (bSynchonize) LOG(INFO) << "-> removed from destination = " << total_emldeleted;
            }

            if (bActionCompact) {
                LOG(INFO) << "-> emails in " <<  total_compact_files << " compact files = " << total_compact_emails;
            }

            if (bActionSplit) {
                LOG(INFO) << "-> emails in split files = " << total_split_emails;
                LOG(INFO) << "-> number of split files = " << total_split_files;
            }

            if (!host_url.empty()) {
                LOG(INFO) << "-> uploads succeed = " << total_upload_succeed;
                LOG(INFO) << "-> uploads failed = " << total_upload_failed;
            }
        }
        LOG(INFO) << "ENDING mboxzilla";
    }
    catch (const std::exception& ex) {
        LOG(ERROR) << "Global exception : " << ex.what();
        return 1;
    }

    std::cout << std::endl;
    return 0;
}

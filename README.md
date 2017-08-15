# mboxzilla
Export / upload emails from Thunderbird mbox files to single eml files

- [Feature Summary](#feature-summary)
- [Usage](#usage)
  - [Command line options](#command-line-options)
  - [Examples](#examples)
- [Informations and advices](#informations-and-advices)
- [How to build](#how-to-build)

## Feature summary

mboxzilla is a free sofware designed to :

- **extract emails** from mbox file to single eml files
- **compact a mbox file** by removing all emails marked as deleted as well as malformed
- **split a mbox** file into smaller mbox files
- **upload extracted emails** to a remote directory in a safe mode
- apply the above tasks **automatically for Mozilla Thunderbird**
- eml files can be compressed in gzip format
- logging can be enabled
- supported platforms : Windows, Linux
- 2-Clause BSD License

## Usage

### Command line options

```
  mboxzilla [OPTION...] [CONFIG_FILE]

  -f, --file FILE             Input mbox file.
  -o, --output DIR            Output directory.
  -p, --path PATH             Base input path where mbox file is stored. When
                              is set then input mbox specified by 'f' option
                              is concatenated to output directory. If PATH is
                              a substring from input mbox file then it is
                              from string following the path argument.
  -e, --extract               Extract emails from mbox file to eml files
                              named 'YYYYmmddHHMMSS_MD5.eml' (or eml.gz) to a
                              local directory.
  -c, --compact               Compact mbox file to file whose name is
                              formatted in 'mboxfilename_YYYYmmddHHMMSS'.
  -s, --split N               Split mbox file into several mbox files of
                              maximum N bytes size. This smaller files are
                              named 'mboxfilename.#' where # is an increment
                              number with auto leading zero if necessary.
  -a, --auto                  Automatic mbox files search and parse for
                              Mozilla Thunderbird client. The search is
                              performed in the current user directory.
      --with-localfolders     Force processing the 'Thunderbird local
                              folders' directory for all profiles selected.
                              Only used with 'auto' option.
      --force-user USER       Set the user name profile for Thunderbird
                              search. Only used with 'auto' option.
      --email-domain DOMAIN   Keep only corresponding accounts found in
                              Thunderbird. Only used with 'auto' option.
      --source-exclude REGEX  Exclude mbox files from Thunderbird list. This
                              is a insensitive regex list separated by comma.
                              Only used with 'auto' option.
  -w, --windows-format        Convert extracted eml files to windows format.
      --synchronize           Synchonize eml files from available emails list
                              and if 'auto' is set then keep only valid
                              Thunderbird directories.
  -z, --compress              Compress eml in gzip format and add extension
                              '.gz' to file name.
  -i, --with-invalid          Invalid emails are retained. This status is
                              defined when at least one of the 'date' or
                              'from' fields is missing from the header. The
                              date filters do not affect these emails. The
                              names are formatted in '00000000000000_MD5.eml'
                              (or eml.gz).
  -x, --with-duplicated       Duplicated emails are retained during
                              processing.
  -d, --with-deleted          Deleted emails are retained during processing.
                              This status is linked to the 'X-Mozilla-Status'
                              header field and therefore only available with
                              Mozilla Thunderbird email client.
  -u, --url URL               Url for the messages uploading process in eml
                              or gz file format. This option require option
                              'k' to be set to trigger the remote sending
                              process. It is independent of 'e' option.
  -k, --key KEY               Password used to secure exchanges with the
                              remote host.
      --age-min N             Select emails that have more than N days.
      --age-max N             Select emails that have less than N days.
      --date-before DATE      Select emails before the specified date. DATE
                              must be formatted in 'YYYY-mm-dd HH:MM:SS' or
                              'YYYY/mm/dd HH:MM:SS'. The parameters
                              'HH:MM:SS' are optional and would be set to
                              '00:00:00' if it is missing.
      --date-after DATE       Select emails after the specified date. Same
                              syntax as 'date-before'
      --timeout N             Set maximum time in seconds the remote
                              connection request is allowed to take. Used if
                              'u' option is set. WARNING: If defined to 0
                              then process could hang. (default: 600)
      --speed-limit N         Set maximum speed in bytes per second to send a
                              file. Used if 'u' option is set. (default: 0)
      --start-wait N          Delay process waiting to start in seconds.
                              (default: 0)
      --start-random N        Maximum delay before process start in seconds.
                              The random value is added to the 'wait'
                              countdown. (default: 0)
      --log-file FILE         Log what we're doing to the specified FILE.
      --log-maxfiles N        Maximum number of log files. Each log file size
                              does not exceed 1 MB. (default: 5)
  -v, --verbose [=N(=3)]      Verbose level for eml processing (N between 1
                              and 3, 3 is implicit). 1=ERROR, 2=WARNING,
                              3=INFO.
      --help                  Display command line options.
```

### Examples

- Simulate an mbox file or a full automatic Thunderbird processing

      mboxzilla -f inbox
      mboxzilla -a
  
- Extract all messages to local folder as eml files
  
      mboxzilla -f inbox -o backup_dir -e
  
- Extract all messages younger than 30 days to local folder
  
      mboxzilla -f inbox -o backup_dir -e --age-max=30
  
- Upload all messages found in Thunderbird profiles with :

     - EML compression to gzip = active
     - Emails age max = 365 days
     - Email profile filter = domain.tld
     - Mbox excluded = trash.*,drafts.*,junk.*,templates.*,(.*/)?private.*
     - Include Thunderbird local folders = yes
     - Host URL = https://server_url/mails/
     - Secure key = _password
     - Synchronization = active
     - Upload max speed = 32768 B/s (256 Kb/s)
     - Verbose level = 2 (Errors and warnings)
     - Log file = mboxzilla.log

  ```
  mboxzilla -a -z --age-max=365 --email-domain "domain.tld"
        --source-exclude "trash.*,drafts.*,junk.*,templates.*,(.*/)?private.*"
        --with-localfolders -u "https://server_url/mails/" -k "_password"
        --synchronize --speed-limit=32768 -v 2 --log-file=mboxzilla.log
  ```
  OR
  
  ```mboxzilla settings.conf``` where configuration file contains : 
     ```
        # mboxzilla configuration example
        auto=true
        compress=true
        age-max=365
        email-domain=domain.tld
        source-exclude=trash.*,drafts.*,junk.*,templates.*,(.*/)?private.*
        with-localfolders=true
        url=https://server_url/mails/
        key=_password
        synchronize=true
        speed-limit=32768
        verbose=2
        log-file=mboxzilla.log
     ```
     
  Web server requires:
    - a file to manage requests in https://server_url/mails/ (see index.php in "server" directory)
    - a subdirectory in "mails/" to store the exported files (see $target_dir value in index.php)
    - to set the key to decrypt eml files (see $key value)
  
## Informations and advices
  - The mbox source files are read-only access and so are never modified.
  - If 'auto' option is set then output directory for Thunderbird is
    'username/profile/name@domain.tld' or 'username/profile/Local Folders'.
  - The (not default) synchronization process works on all the eml files but for
    directories sync it's only applied for Thunderbird.
  - Remotely exported files are transferred using AES-256-CBC encryption mode
  - Thunderbird IMAP type accounts are ignored.
  - If an error occurred while parsing a mbox then its processing is aborted
    and goes to next one. In this case there is no files synchronization.
  
  - This software uses the following external libraries :
    * CXXOPTS - Copyright (c) 2014-2016 Jarryd Beck\
        -> source: https://github.com/jarro2783/cxxopts
    * EASYLOGGING++ - Copyright (c) 2016 muflihun.com\
        -> source: https://github.com/easylogging/easyloggingpp
    * JSON for Modern C++ - Copyright (c) 2013-2016 Niels Lohmann\
        -> source: https://github.com/nlohmann/json
    * SIMPLEINI - Copyright (c) 2006-2012 Brodie Thiesfield\
        -> source: https://github.com/brofield/simpleini

## How to build

The build requirements are:
- C++ compiler that supports C++11 regular expressions. For example GCC >= 4.9 or clang with libc++.
- C++ Libraries : zlib, ssh2, ssl, curl

From linux do :

  - linux binary:
    ```
    g++ -Os -s -std=c++11 mboxzilla.cpp mbox_parser.cpp common.cpp easylogging++.cc -o bin/linux/mboxzilla -lcrypto -lcurl -lz -DELPP_NO_DEFAULT_LOG_FILE
    ```
  - windows executable:
    ```
    i686-w64-mingw32-g++ -static -Os -s -std=c++11 mboxzilla.cpp mbox_parser.cpp common.cpp easylogging++.cc -o bin/win32/mboxzilla.exe -lcurl -lssl -lssh2 -lcrypto -lz -lws2_32 -lwldap32 -lwinmm -lgdi32 -DCURL_STATICLIB -DELPP_NO_DEFAULT_LOG_FILE && ./tools/upx.exe bin/win32/mboxzilla.exe
    ```
The **Mbox_parser** class can be freely used outside this project.

/*
    BSD 2-Clause License

    Copyright (c) 2017, Noël Martinon
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

#include "common.hpp"

//---------------------------------------------------------------------------------------------
/**
 *  NoCaseLess()
 *  To use with sort() to have a insensitive sorted list
 *    EXAMPLE :
 *      sort(v.begin(),v.end(),NoCaseLess);            
 *      copy(v.begin(),v.end(),ostream_iterator<string>(cout,"\n")); // to view result
 */
struct case_insensitive_less : public std::binary_function< char,char,bool >
{
    bool operator () (char x, char y) const
    {
        return toupper( static_cast< unsigned char >(x)) < 
               toupper( static_cast< unsigned char >(y));
    }
};

bool NoCaseLess(const std::string &a, const std::string &b)
{
    return std::lexicographical_compare( a.begin(),a.end(),
            b.begin(),b.end(), case_insensitive_less() );
}
//---------------------------------------------------------------------------------------------
/**
 *  FileExists()
 *  Check if a regular file exists
 *  Returns true if the file exists
 */
bool FileExists(const std::string& path) {

    struct stat path_stat;
    return (stat(path.c_str(), &path_stat) == 0 && S_ISREG(path_stat.st_mode));
}
//---------------------------------------------------------------------------------------------
/**
 *  DirectoryExists()
 *  Check if a directory exists
 *  Returns true if the directory exists
 */
bool DirectoryExists(const std::string& path) {

    struct stat path_stat;
    return (stat(path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode));

}
//---------------------------------------------------------------------------------------------
/**
 *  ListDirectoryContents()
 *  List directory contents to a string vector
 *  Filters on files or directories can be apply. By default these are the two.
 */
bool ListDirectoryContents(std::vector<std::string>& vList, const std::string directory, bool bGetFiles, bool bGetDirectories) {

    if (!bGetFiles && !bGetDirectories) return false;
    vList.clear();
    DIR *dir;
    struct dirent *ent;

    if (DirectoryExists(directory) && (dir = opendir (directory.c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            bool is_dir;
            bool is_file;
            if (strcmp(ent->d_name, ".") == 0 || // If file is "."
                strcmp(ent->d_name, "..") == 0) //  or ".."
                continue;

            #ifdef _DIRENT_HAVE_D_TYPE
                if (ent->d_type != DT_UNKNOWN && ent->d_type != DT_LNK) {
                   // don't have to stat if we have d_type info, unless it's a symlink (since we stat, not lstat)
                   is_dir = (ent->d_type == DT_DIR);
                   is_file = (ent->d_type == DT_REG);
                } else
            #endif
                // Cross platform code is used to avoid problems
                // (eg: bad value on get S_ISDIR or S_IFDIR from stat(ent->st_mode, &stbuf) on Windows)
                {
                   is_dir = DirectoryExists(directory+"/"+ent->d_name);
                   is_file = FileExists(directory+"/"+ent->d_name);
                }

        if ( (bGetDirectories && is_dir) || (bGetFiles && is_file) )
            vList.push_back(ent->d_name);

        }
        closedir (dir);
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  ListAllSubDirectories()
 *  List all sub-directories in a directory
 */
bool ListAllSubDirectories(std::vector<std::string>& vList, const std::string directory) {

    std::vector<std::string> vListCurrent;
    ListDirectoryContents(vListCurrent, directory, false, true);

    for(auto n : vListCurrent){
        vList.push_back(directory+"/"+n);
        ListAllSubDirectories(vList, directory+"/"+n);
    }
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  PrintMD5()
 *  Hash a string to MD5
 */
std::string PrintMD5(std::string str) {

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    unsigned char result[MD5_DIGEST_LENGTH];
    const char *cstr = str.c_str();

    MD5((const unsigned char*)cstr, str.length(), result);

    for (size_t i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        ss << std::setw(2) << static_cast<unsigned>(result[i]);
    }

    return ss.str();
}
//---------------------------------------------------------------------------------------------
/**
 *  offset()
 *  Returns index of a search string in a vector of char arrays
 */
size_t offset(std::vector<char> &haystack, std::string const &str, size_t index) {

    if (index>=haystack.size()) return -1;
    std::vector<char>::iterator it;
    std::vector<char> needle(str.begin(), str.end());
    it = std::search (haystack.begin()+index, haystack.end(), needle.begin(), needle.end());
    if ( it != haystack.end() ) return it - haystack.begin();
    else return -1;
}
//---------------------------------------------------------------------------------------------
/**
 *  ci_offset()
 *  Returns index of a search string in a vector of char arrays. The search is insensitive.
 */
size_t ci_offset(std::vector<char> &haystack, std::string const &str, size_t index) {

    std::vector<char> needle(str.begin(), str.end());
    return ci_find_substr(haystack, needle, index);
}
//---------------------------------------------------------------------------------------------
/**
 *  split()
 *  Split a string to vector of string arrays
 */
void split(const std::string& s, char c, std::vector<std::string>& v, bool allowEmptyString) {

    std::string::size_type i = 0;
    std::string::size_type j = s.find(c);
    v.clear();

    while (j != std::string::npos) {
        std::string str = s.substr(i, j-i);
        if (allowEmptyString || str.length()) v.push_back(str);
        i = ++j;
        j = s.find(c, j);

        if (j == std::string::npos){
            str = s.substr(i, s.length());
            if (allowEmptyString || str.length()) v.push_back(str);
        }
   }
}
//---------------------------------------------------------------------------------------------
/**
 *  get_month_num()
 *  Returns the month number
 */
int get_month_num( std::string name ) {

    std::map<std::string, int> months
    {
        { "jan", 1 },
        { "feb", 2 },
        { "mar", 3 },
        { "apr", 4 },
        { "may", 5 },
        { "jun", 6 },
        { "jul", 7 },
        { "aug", 8 },
        { "sep", 9 },
        { "oct", 10 },
        { "nov", 11 },
        { "dec", 12 },
        { "Jan", 1 },
        { "Feb", 2 },
        { "Mar", 3 },
        { "Apr", 4 },
        { "May", 5 },
        { "Jun", 6 },
        { "Jul", 7 },
        { "Aug", 8 },
        { "Sep", 9 },
        { "Oct", 10 },
        { "Nov", 11 },
        { "Dec", 12 },
        { "JAN", 1 },
        { "FEB", 2 },
        { "MAR", 3 },
        { "APR", 4 },
        { "MAY", 5 },
        { "JUN", 6 },
        { "JUL", 7 },
        { "AUG", 8 },
        { "SEP", 9 },
        { "OCT", 10 },
        { "NOV", 11 },
        { "DEC", 12 }
    };

    const auto iter = months.find( name );

    if( iter != months.cend() )
        return iter->second;
    return -1;
}
//---------------------------------------------------------------------------------------------
/**
 *  get_day_index()
 *  Returns the number of the day in the week (monday is 1)
 */
int get_day_num( std::string name ) {

    std::map<std::string, int> months
    {
        { "mon", 1 },
        { "tue", 2 },
        { "wed", 3 },
        { "thu", 4 },
        { "fri", 5 },
        { "sat", 6 },
        { "sun", 7 },
        { "Mon", 1 },
        { "Tue", 2 },
        { "Wed", 3 },
        { "Thu", 4 },
        { "Fri", 5 },
        { "Sat", 6 },
        { "Sun", 7 },
        { "MON", 1 },
        { "TUE", 2 },
        { "WED", 3 },
        { "THU", 4 },
        { "FRI", 5 },
        { "SAT", 6 },
        { "SUN", 7 }
    };

    const auto iter = months.find( name );

    if( iter != months.cend() )
        return iter->second;
    return -1;
}
//---------------------------------------------------------------------------------------------
/**
 *  getDiffTime()
 *  Returns difference between two dates in seconds (strdate2 - strdate1)
 *  If strdate2 is undefined then strdate2 = date now
 */
time_t getDiffTime(std::string strdate1, std::string strdate2) {

    time_t diff;
    tm tm_time1, tm_time2;
    time_t tt_time1, tt_time2;
    int Y,M,d,h=0,m=0,s=0;

    int retval = sscanf(strdate1.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
    if (retval<3) retval = sscanf(strdate1.c_str(), "%d/%d/%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
    if (retval<3) return 0;
    tm_time1.tm_year = Y - 1900; // Year since 1900
    tm_time1.tm_mon = M - 1;     // 0-11
    tm_time1.tm_mday = d;        // 1-31
    tm_time1.tm_hour = h;        // 0-23
    tm_time1.tm_min = m;         // 0-59
    tm_time1.tm_sec = s;
    tt_time1 = mktime(&tm_time1);

    if (!strdate2.empty()){
        h=0,m=0,s=0;
        int retval = sscanf(strdate2.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
        if (retval<3) retval = sscanf(strdate2.c_str(), "%d/%d/%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
        if (retval<3) return 0;
        tm_time2.tm_year = Y - 1900; // Year since 1900
        tm_time2.tm_mon = M - 1;     // 0-11
        tm_time2.tm_mday = d;        // 1-31
        tm_time2.tm_hour = h;        // 0-23
        tm_time2.tm_min = m;         // 0-59
        tm_time2.tm_sec = s;
        tt_time2 = mktime(&tm_time2);
    }
    else {
        tt_time2 = time(0);
    }

    diff = tt_time2 - tt_time1;

    return diff;

}
//---------------------------------------------------------------------------------------------
/**
 *  checkdate()
 *  Check if date exists
 */
bool checkdate(int d, int m, int y) {

  //gregorian dates started in 1582
  if (! (1582<= y )  )//comment these 2 lines out if it bothers you
     return false;
  if (! (1<= m && m<=12) )
     return false;
  if (! (1<= d && d<=31) )
     return false;
  if ( (d==31) && (m==2 || m==4 || m==6 || m==9 || m==11) )
     return false;
  if ( (d==30) && (m==2) )
     return false;
  if ( (m==2) && (d==29) && (y%4!=0) )
     return false;
  if ( (m==2) && (d==29) && (y%400==0) )
     return true;
  if ( (m==2) && (d==29) && (y%100==0) )
     return false;
  if ( (m==2) && (d==29) && (y%4==0)  )
     return true;

  return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  checktime()
 *  Check if time exists
 */
bool checktime(int h, int m, int s=0) {

    if (h<0 || h>23 || m<0 || m>59 || s<0 || s>59) return false;
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  createPath()
 *  create recursive directories
 */
bool createPath( std::string path, mode_t mode ) {

    if (DirectoryExists(path)) return true;
    if (path.empty()) return false;

    // change Windows '\' to '/'
    std::replace( path.begin(), path.end(), '\\', '/');

    // Ensure path ending with '/'
    if (!path.empty() && *path.rbegin() != '/') path += '/';

    struct stat st;
    for( std::string::iterator iter = path.begin() ; iter != path.end(); iter++ )
    {
        std::string::iterator newIter = std::find( iter, path.end(), '/' );
        std::string newPath = std::string( path.begin(), newIter);

        if (!newPath.empty()) {

            if( stat( newPath.c_str(), &st) != 0)
            {
                #ifdef __linux__
                if( mkdir( newPath.c_str(), mode) != 0 && errno != EEXIST )
                #else
                if( mkdir( newPath.c_str()) != 0 && errno != EEXIST )
                #endif
                {
                    return false;
                }
            }

            else if( !(st.st_mode & S_IFDIR) )
            {
                errno = ENOTDIR;
                return false;
            }
        }
        iter = newIter;
    }
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  match()
 *  Checks if two given strings match.
 *  The wild string may contain wildcard characters :
 *  * --> Matches with 0 or more instances of any character or set of characters.
 *  ? --> Matches with any one character.
*/
bool match(const char *wild, const char * str) {

    // If we reach at the end of both strings, we are done
    if (*wild == '\0' && *str == '\0')
        return true;

    // Make sure that the characters after '*' are present
    // in str string. This function assumes that the wild
    // string will not contain two consecutive '*'
    if (*wild == '*' && *(wild+1) != '\0' && *str == '\0')
        return false;

    // If the wild string contains '?', or current characters
    // of both strings match
    if (*wild == '?' || *wild == *str)
        return match(wild+1, str+1);

    // If there is *, then there are two possibilities
    // a) We consider current character of str string
    // b) We ignore current character of str string.
    if (*wild == '*')
        return match(wild+1, str) || match(wild, str+1);
    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  trim()
 *  Strip whitespace (or other characters) from the beginning and end of a string
 *  By default whitespace includes space and tabulation (" \t")
 */
std::string trim(const std::string& str, const std::string& whitespace) {

    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}
//---------------------------------------------------------------------------------------------
/**
 *  reduce()
 *  An advanced trim funtion that replaces char into a string
 *  eg:
 *      const std::string foo = "    too much\t  spa\tce\t\t\t  ";
 *      std::cout << "[" << reduce(foo) << "]" << std::endl;
 *      std::cout << "[" << reduce(foo, "-") << "]" << std::endl;
 *      OUTPUT :
 *          [too much space]
 *          [too-much-space]
 */
std::string reduce(const std::string& str,
                   const std::string& fill,
                   const std::string& whitespace) {

    // trim first
    auto result = trim(str, whitespace);

    // replace sub ranges
    auto beginSpace = result.find_first_of(whitespace);
    while (beginSpace != std::string::npos)
    {
        const auto endSpace = result.find_first_not_of(whitespace, beginSpace);
        const auto range = endSpace - beginSpace;

        result.replace(beginSpace, range, fill);

        const auto newStart = beginSpace + fill.length();
        beginSpace = result.find_first_of(whitespace, newStart);
    }

    return result;
}
//---------------------------------------------------------------------------------------------
/**
 *  str_replace()
 *  Replace all occurrences of the search string with the replacement string
 */
void str_replace(std::string& str, const std::string& from, const std::string& to) {

    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}
//---------------------------------------------------------------------------------------------
/**
 *  count_needle()
 *  Count the number of occurrence in a string (needle in a haystack)
 */
size_t count_needle(std::string const &haystack, std::string const &needle) {

    int occurrences = 0;
    size_t len = needle.size();
    size_t pos = 0;

    while (std::string::npos != (pos = haystack.find(needle, pos))) {
        ++occurrences;
        pos += len;
    }
    return occurrences;
}
//---------------------------------------------------------------------------------------------
/**
 *  count_needle()
 *  Count the number of occurrence in a string vector
 */
size_t count_needle(std::vector<std::string> const &haystack, std::string const &needle) {

    int occurrences = 0;

    //while (std::find(haystack.begin(), haystack.end(), needle) != haystack.end()) {
    for (auto& str : haystack) {
        if (str == needle) ++occurrences;
    }
    return occurrences;
}
//---------------------------------------------------------------------------------------------
/**
 *  path_dusting()
 *  Reformat a path string :
 *      all characters '\\' are replaced by '/'
 *      and all "//" by "/"
 */
std::string path_dusting (const std::string path) {

    std::string path_dust = path;
    std::replace( path_dust.begin(), path_dust.end(), '\\', '/');

    while (path_dust.find("//") != std::string::npos)
        str_replace(path_dust, "//", "/");

    return path_dust;
}
//---------------------------------------------------------------------------------------------
/**
 *  bytes_convert()
 *  Convert a double bytes value in formatted string KB,MB,GB,TB,PB
 */
std::string bytes_convert(double bytes) {

    std::ostringstream oss;
    if (bytes >= pow(1024,5)) oss << floor(bytes/pow(1024,5)*100)/100 << " PB" ;
    if (bytes >= pow(1024,4)) oss << floor(bytes/pow(1024,4)*100)/100 << " TB" ;
    if (bytes >= pow(1024,3)) oss << floor(bytes/pow(1024,3)*100)/100 << " GB" ;
    else if (bytes >= 1024*1024) oss << floor(bytes/(1024*1024)*100)/100 << " MB" ;
    else if (bytes >= 1024) oss << floor(bytes/1024*100)/100 << " KB" ;
    else oss << bytes << " B" ;
    return oss.str();
}
//---------------------------------------------------------------------------------------------
/**
 *  is_number()
 *  Returns true if string is a number
 */
bool is_number(const std::string& s) {
    
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}
//---------------------------------------------------------------------------------------------
/**
 *  is_asctime()
 *  Test if string is in POSIX asctime format 'Www Mmm dd hh:mm:ss yyyy'.
 *  If 'strict' is set to false then the test is permissive and string as
 *  'Www Mmm d hh:mh:ss yyyy' or 'Www Mmm d h:m:s yyyy' are accepted.
 *      eg: 'Thu Jan 02 15:37:45 2014'  in strict mode
 *          'Mon Oct 7 5:37:45 2011'    in permissive mode
 *  Returns true if string is in (permissive) asctime format AND date/time exists.
 */
bool is_asctime(std::string s, bool strict) {

    int dayweek;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int year;

    if (strict || s.length()==24) {
        if (s.length()!=24) return false; // case strict: size must be 24
        if (s[3]!=' ' || s[7]!=' ' || s[10]!=' ' || s[13]!=':' || s[16]!=':' || s[19]!=' ') return false;
        std::string s_dayweek = s.substr(0,3);
        std::string s_month = s.substr(4,3);
        std::string s_day = s.substr(8,2);
        std::string s_hour = s.substr(11,2);
        std::string s_min = s.substr(14,2);
        std::string s_sec = s.substr(17,2);
        std::string s_year = s.substr(20,4);

        dayweek = get_day_num(s_dayweek);
        month = get_month_num(s_month);
        day = std::stoi(s_day);
        hour = std::stoi(s_hour);
        min = std::stoi(s_min);
        sec = std::stoi(s_sec);
        year = std::stoi(s_year);
    }
    else {
        std::vector<std::string> v;
        std::vector<std::string> vtime;

        split(s, ' ', v, false);
        if (v.size()!=5) return false;

        split(v[3], ':', vtime, false);
        if (vtime.size()!=3) return false;

        dayweek = get_day_num(v[0]);
        month = get_month_num(v[1]);
        day = std::stoi(v[2]);
        hour = std::stoi(vtime[0]);
        min = std::stoi(vtime[1]);
        sec = std::stoi(vtime[2]);
        year = std::stoi(v[4]);
    }

    if (dayweek==-1) return false;
    if (!checkdate(day, month, year)) return false;
    if (!checktime(hour, min, sec)) return false;

    return true;
}
//---------------------------------------------------------------------------------------------

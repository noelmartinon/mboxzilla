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

#include "mbox_parser.hpp"
#include "common.hpp"

using namespace std;

/**
 *  Settings for progress animation
 */
const char *Mbox_parser::Anim[] = {"-", "\\", "|", "/"};
float Mbox_parser::iAnim=0;

//---------------------------------------------------------------------------------------------
/**
 *  Class constructor
 *  Initialize global settings
 */
Mbox_parser::Mbox_parser(std::string const filename) {

    if (!filename.empty()) SetMboxFile(filename);

    // Variables to maintain when on re-Init() call
    mailAgeMin = 0;
    mailAgeMax = 0;
    tt_maildatebefore = 0;
    tt_maildateafter = 0;
    outputdirectory = "";
    bEmlToWindows = false;
    bSynchronize = false;
    bGenerateMboxCompact = false;
    bExtractMboxEml = false;
    bGenerateMboxSplit = false;
    bCompressEml = false;
    bExtractInvalid = false;
    bExtractDeleted = false;
    bExtractDuplicated = false;
    mboxsplitmaxsize = 0;
    GetLocalTimeZone();
    cbFunc_eml_preprocess = NULL;
    cbFunc_eml_process = NULL;
    cbFunc_log = NULL;
    readytoparse = false;
}
//---------------------------------------------------------------------------------------------
/**
 *  Class destructor
 */
Mbox_parser::~Mbox_parser() {}
//---------------------------------------------------------------------------------------------
/**
 *  SetMboxFile()
 *  Set the mbox source filename and open file
 *  Return true if file is mbox type and is ready to parse
 */
bool Mbox_parser::SetMboxFile(std::string const filename){

    readytoparse = false;
    mboxfile.close(); // Ensure file is closed - Useful in recursive call if throw exception

    if (filename.empty()){
        if (*cbFunc_log) cbFunc_log ("ERROR", "No mbox file defined");
        return false;
    }

    // Set full path + mbox file
    mboxfullname = filename;

    // Set mbox file name only
    mboxfilename = filename;
    std::replace( mboxfilename.begin(), mboxfilename.end(), '\\', '/'); // Replace Windows '\' to '/'

    size_t pos = mboxfilename.find_last_of("/");
    if (pos != string::npos)
        mboxfilename = mboxfilename.substr(pos+1);

    mboxfile.open( mboxfullname, std::ifstream::binary );//std::ios::binary
    if (mboxfile.fail()) {
        if (*cbFunc_log) cbFunc_log ("ERROR", "Failed to open mbox file \""+mboxfullname+"\"");
        return false;
    }
    mboxfile.seekg (0, mboxfile.end);
    mboxlength = mboxfile.tellg();
    mboxfile.seekg (0, mboxfile.beg);

    // Init buffer for recursive call on same Mbox_parser object.
    // Required when mbox file size is small than buffer size.
    buffer.assign(1024*1024,0);

    // Start reading file
    mboxfile.read(buffer.data(), buffer.size());
    if (!IsMboxFile()) {
        mboxfile.close();
        if (*cbFunc_log) cbFunc_log ("ERROR", "Input file is not mbox type : \""+mboxfullname+"\"");
        return false;
    }

    readytoparse = true;
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  IsReadyToParse()
 *  Return true if input file is ready to be parsed
 */
bool Mbox_parser::IsReadyToParse(){

    return readytoparse;
}
//---------------------------------------------------------------------------------------------
/**
 *  Init()
 *  Reset settings to proceed mbox file parsing
 *  Always called before Parse()
 */
void Mbox_parser::Init() {
    mboxindex = 0;
    nbmailread = 0; // mails read in mbox
    nbmailok = 0; // mails availables that are proccessed
    nbmailinvalid = 0;
    nbmaildeleted = 0;
    nbmailduplicated = 0;
    nbmailexcluded = 0;
    nbmailextracted = 0;
    nbmailcompact = 0;
    nbmailsplit = 0;
    nbsplitfile = 0;
    nbemlremoved = 0;
    bDisableMboxCompact = false;
    mboxsplitcurrentsize = 0;
    bDisableMboxSplit = false;
    splitfilename = "";
    splitindex = 0;
    vmails.clear();
    vmail.clear();
    vheader.clear();
    vmailcrlf.clear();
    tt_timezero = time(0);
    islastmail = false;
    bmaildatestored = false;
    tt_maildate = 0;
    newline = "\n";
    iAnim=0;
    emlfilename = "";
    emlList.clear();
}
//---------------------------------------------------------------------------------------------
/**
 *  ShowProgressBar()
 *  Display console progress bar for current mbox file
 */
void Mbox_parser::ShowProgressBar() {
    std::cout << "[" << std::string(floor(this->i_progression/2), '=') << std::string(50-floor(this->i_progression/2), ' ') << "] ";
    std::cout << std::setw(3) << i_progression << "% " << Anim[(int)floor(iAnim)] << "\r";
    std::cout.flush();
    iAnim +=.50;
    if (iAnim >= 4) iAnim = 0;
    usleep(1000);
}
//---------------------------------------------------------------------------------------------
/**
 *  IsMboxFile()
 *  Return true if source file is mbox type
 *  This is only correct for the first buffer
 */
bool Mbox_parser::IsMboxFile() {
    if (offset(buffer, "From ")!=0) return false;
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  GetLocalTimeZone()
 *  Store result from time difference between local and gmt time
 */
void Mbox_parser::GetLocalTimeZone() {
    time_t now = time(0); // UTC
    time_t diff;
    struct tm *ptmgm = gmtime(&now); // further convert to GMT presuming now in local
    time_t gmnow = mktime(ptmgm);
    diff = now - gmnow;
    hourlocalTZ = (diff / 3600) % 24;
    minutelocalTZ = (diff / 60) % 60;
}
//---------------------------------------------------------------------------------------------
/**
 *  Parse()
 *  Process the source file by block
 *  Return nb of valid emails according to optional filters applied
 */
int Mbox_parser::Parse() {

    if (!readytoparse){
        // Case Parse() is calling just after Mbox_parser constructor without input filename
        if (mboxfullname.empty()){
            if (*cbFunc_log) cbFunc_log ("ERROR", "Unable to parse undefined input file");
        }
        // Case Parse() is calling just after Mbox_parser constructor without mbox file
        else if (!mboxfile.is_open()){
            if (*cbFunc_log) cbFunc_log ("ERROR", "Unable to parse file \""+mboxfullname+"\"");
        }
        return -1;
    }

    this->Init();

    // Create output directory if necessary
    if ((bGenerateMboxCompact || bExtractMboxEml || (bGenerateMboxSplit && mboxsplitmaxsize)) && !DirectoryExists(outputdirectory)) {
        if (outputdirectory.empty()) {
            mboxfile.close();
            if (*cbFunc_log) cbFunc_log ("ERROR", "Output directory is undefined");
            return -1;
        }
        else if (!createPath(outputdirectory)) {
            mboxfile.close();
            if (*cbFunc_log) cbFunc_log ("ERROR", "Output directory cannot be created : \""+outputdirectory+"\"");
            return -1;
        }
    }

    if (bGenerateMboxCompact) {
        std::stringstream ss;
        ss << std::put_time(std::localtime(&tt_timezero), "_%Y%m%d%H%M%S");
        compactfilename = outputdirectory + mboxfilename + ss.str();
        outputcompact.open( compactfilename, std::ofstream::binary | std::ofstream::app );
        if (! outputcompact.is_open()){
             mboxfile.close();
             if (*cbFunc_log) cbFunc_log ("ERROR", "Could not open \""+compactfilename+"\". Compact process is aborted.");
             bDisableMboxCompact = false;
        }
    }

    tt_timezero = time(0);
    if (mailAgeMax>0) SetAgeMax(mailAgeMax);
    if (mailAgeMin>0) SetAgeMin(mailAgeMin);

    if (tt_maildateafter>0){
        std::stringstream ss;
        ss << std::put_time(std::localtime(&tt_maildateafter), "Apply filter \"AFTER %a %b %d %H:%M:%S %Y\"");
        if (*cbFunc_log) cbFunc_log ("INFO", ss.str());
    }

    if (tt_maildatebefore>0){
        std::stringstream ss;
        ss << std::put_time(std::localtime(&tt_maildatebefore), "Apply filter \"BEFORE %a %b %d %H:%M:%S %Y\"");
        if (*cbFunc_log) cbFunc_log ("INFO", ss.str());
    }

    if (*cbFunc_log) cbFunc_log ("INFO", "Start parsing file \""+mboxfullname+"\"");

    while(mboxfile.gcount()) {
        mboxindex += mboxfile.gcount();
        this->i_progression=100*((float)mboxindex/(float)mboxlength);

        ProcessPacket();
        mboxfile.read(buffer.data(), buffer.size());
    }

    cout << std::string(59, ' ') << "\r";

    mboxfile.close();
    readytoparse = false;

    // Synchronize output directory content
    if (bSynchronize && bExtractMboxEml) {
        std::vector<string> vListDirectory;

        // List files (only) contains in directory output
        if (ListDirectoryContents(vListDirectory, outputdirectory, true, false)){
            vector<string> vListDiff;

            sort(vListDirectory.begin(), vListDirectory.end());
            sort(emlList.begin(), emlList.end());

            set_difference(vListDirectory.begin(),vListDirectory.end(),emlList.begin(),emlList.end(),back_inserter(vListDiff));


            for(string n : vListDiff){
                n = outputdirectory + n;
                int ret = std::remove(n.c_str());
                if (!ret) nbemlremoved++;
                if (*cbFunc_log){
                    if (!ret) cbFunc_log ("INFO", "File \""+n+"\" was deleted");
                    else cbFunc_log ("WARNING", "Can not delete file \""+n+"\"");
                }
            }
        }
    }

    // if output directory is empty then delete it
    std::vector<string> vList;
    ListDirectoryContents(vList, outputdirectory, true, true);
    if (vList.empty()) std::remove(outputdirectory.c_str());

    if (*cbFunc_log) cbFunc_log ("INFO", "End parsing and processing file");

    return nbmailok;
}
//---------------------------------------------------------------------------------------------
/**
 *  FindMailSeparator()
 *  Search mbox email's separator with MBOX Email Format define as :
 *  Each message in mbox format begins with a line beginning with the string "From "(ASCII characters F, r, o,
 *  m, and space). "From" lines are followed by several more fields: envelope-sender, date, and (optionally)
 *  more-data. The date field is in standard UNIX asctime() format and is always 24 characters in length.
 *  It is formatted as 'Www Mmm dd hh:mm:ss yyyy'
 *
 *  After several tests, it turns out that sometimes the format is 'Www Mmm d hh:mm:ss yyyy' because of the
 *  only one digit in the day of the month. The function supports this case.
 */
bool Mbox_parser::FindMailSeparator(bool bUseAsctime) {

    // If vmails is empty then this the end of the mbox file
    if (!vmails.size()) return false;

    mailsize = offset(vmails, "\nFrom ", 1); // +1 to start search from vmails+1 that always is the "r" of "From"

    while (mailsize!=std::string::npos || (mboxindex == mboxlength && mailsize==std::string::npos)) {
        // If end of mbox file
        if (mboxindex == mboxlength && mailsize==std::string::npos) {
            islastmail=true;
            mailsize=vmails.size();
            return true;
        }
        // Verify that the found separator "From " is a line structure as "From sender date moreinfo"
        // See http://www.digitalpreservation.gov/formats/fdd/fdd000383.shtml
        else if (bUseAsctime) {
            size_t pos = offset(vmails, "\n", mailsize+1);// search '\n' at the end of the line "From "
            if (pos==(size_t)-1) return false;
            if (vmails[pos-1] == '\r') pos--; // pos do not content any newline char
            if (pos-mailsize < 6) return false;

            size_t pos_s = offset(vmails, " ", mailsize+6)+1;// search next space following "From "
            if (pos_s==(size_t)-1) return false;

            // Extract string in order to search asctime date
            const std::string s (vmails.begin()+pos_s,vmails.begin()+pos);
            std::vector<std::string> v;
            split(s , ' ', v, false);
            if (v.size()<5) return false;

            // Check if string is a 'permissive' asctime
            // When verifying with strptime("%a %b %d %H:%M:%S %Y") or even worse with
            // regex "^From .+ .{24}" the global process is slowing.
            // So using a simple string comparaison :
            string date = v[0]+" "+v[1]+" "+v[2]+" "+v[3]+" "+v[4];
            if (!is_asctime(date, false))
                return false;

            return true;
        }
        // Verify if next line is a header field
        else {
            size_t pos_endfrom = offset(vmails, "\n", mailsize+1);// search '\n' at the end of the line "From "
            if (pos_endfrom==(size_t)-1) return false;

            size_t pos = offset(vmails, "\n", pos_endfrom+1);
            if (pos==(size_t)-1) return false;
            if (vmails[pos-1] == '\r') pos--;

            const std::string s (vmails.begin()+pos_endfrom+1,vmails.begin()+pos);
            std::regex rgx("^.+:");
            if (std::regex_match(s, rgx))
                return false;

            return true;
        }
        mailsize = offset(vmails, "\nFrom ", mailsize+1);
    }

    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  ProcessPacket()
 *  Process packet of byte size defined for buffer
 */
void Mbox_parser::ProcessPacket() {

    ShowProgressBar();

    if (!vmails.size()) {
        vmails = buffer;
    }
    else {
        copy(buffer.begin(), buffer.begin()+mboxfile.gcount(), std::back_inserter(vmails));
    }

    while (FindMailSeparator()) {
        vmail.assign(vmails.begin(), vmails.begin()+mailsize+((islastmail)?0:1)); // assign after \n from "\nFrom - "
        vmails.erase(vmails.begin(), vmails.begin()+mailsize+((islastmail)?0:1)); // erase before \n from "\nFrom - "

        nbmailread++;
        ProcessMail();
    }
}
//---------------------------------------------------------------------------------------------
/**
 *  ProcessMail()
 *  Process email (save to eml, create compact, split mbox or callback)
 *  if it is valid, not deleted and not excluded by filter
 */
void Mbox_parser::ProcessMail() {

    ShowProgressBar();

    newline = "\n";
    int pos = offset(vmail, newline+newline);
    if (pos==-1) {
        newline = "\r\n";
        pos = offset(vmail, newline+newline);
    }

    if (pos>=0) {
        // header beginning with "From "
        vheader.assign (vmail.begin(), vmail.begin()+pos+newline.length()); // add one newline for GetHeaderField() that terminated with "\n".
    }
    else {
        vmail.clear();
        return;
    }

    bmaildatestored = false;
    bool bIsValidMail = IsValidMail();
    emlfilename = "";

    if (!bIsValidMail) {
        nbmailinvalid++;

        // if do not extract invalid
        if (!bExtractInvalid) {
            vheader.clear();
            vmail.clear();
            return;
        }
        // if set to be store (even if marked as deleted)
        else {
            // Generate file name based on MD5 content (without any header field)
            std::string str(vmail.begin(),vmail.end());
            emlfilename = "00000000000000_"+PrintMD5(str)+".eml";
            if (bCompressEml) emlfilename += ".gz";
        }
    }
    // if valid and must ignored deleted
    else if (!bExtractDeleted && IsDeletedMail()) {
        nbmaildeleted++;
        vheader.clear();
        vmail.clear();
        return;
    }

    // Not 'else' because it's necessarily a valid email and not marked as deleted
    // If email is invalid and it must extract invalid then IsExcludedMail is ignored
    if (bIsValidMail && IsExcludedMail()) {
        vheader.clear();
        vmail.clear();
        nbmailexcluded++;
        return;
    }

    // Deleted emails are renamed
    if (IsDeletedMail())
        emlfilename = "del_"+EmlFilename();

    // Verifying duplicate email
    int nbdup = count_needle(emlList, EmlFilename());
    if (nbdup > 0)
    {
        nbmailduplicated++;
        if (!bExtractDuplicated) {
            vheader.clear();
            vmail.clear();
            return;
        }
        emlfilename = "dup"+std::to_string(nbdup)+"_"+EmlFilename();
    }


    nbmailok++;
    emlList.push_back(EmlFilename());

    if (DirectoryExists(outputdirectory)) {
        if (bExtractMboxEml) {
            if (!FileExists(outputdirectory + EmlFilename())) {
                if (SaveToEML()) {
                    if (*cbFunc_log) cbFunc_log ("VERBOSE3", "Successfully saved email to \""+outputdirectory + EmlFilename()+"\"");
                    nbmailextracted++;
                }
                else if (*cbFunc_log) cbFunc_log ("VERBOSE1", "Unable to save email to \""+outputdirectory + EmlFilename()+"\"");
            }
            else {if (*cbFunc_log) cbFunc_log ("VERBOSE2", "Already existing file \""+outputdirectory + EmlFilename()+"\"");
                //emlfilename = "_" + EmlFilename();
                //SaveToEML();
            }
        }

        if (bGenerateMboxCompact && !bDisableMboxCompact) {
            if (SaveToCompact()) nbmailcompact++;
        }

        if (bGenerateMboxSplit && !bDisableMboxSplit) {
            if (SaveToSplit()) nbmailsplit++;
        }
    }

    // If callback for eml process is defined
    if (*cbFunc_eml_process) {
        bool valid = true;
        // If callback for previous test of eml preprocess is defined
        if (*cbFunc_eml_preprocess)
            valid = cbFunc_eml_preprocess(outputdirectory, EmlFilename());
        if (valid) {
            StoreEML();
            cbFunc_eml_process(outputdirectory, EmlFilename(), vmailcrlf);
        }
    }

    vmail.clear();
    vheader.clear();
    vmailcrlf.clear();
}
//---------------------------------------------------------------------------------------------
/**
 *  StoreEML()
 *  Save email to vector vmailcrlf in the brut format as it is in the inbox file unless the
 *  "windows-format" option is specified. In this case, the end of line character is forced to "\r\n"
 */
void Mbox_parser::StoreEML(){

    if (vmailcrlf.size()) return;
    int firstline = offset(vmail, "\n")+1;

    if (newline == "\n" && bEmlToWindows) {
        string crlf = "\r\n";
        size_t prevpos = firstline;
        size_t pos = offset(vmail, "\n",firstline);
        while (pos != (size_t)-1) {
            ShowProgressBar();
            vmailcrlf.insert( std::end(vmailcrlf), std::begin(vmail)+prevpos, std::begin(vmail)+pos );
            if (vmailcrlf.back() == '\r') vmailcrlf.pop_back(); // Sometimes the extracted email contains a mix of linux and windows line breaks
            vmailcrlf.insert( std::end(vmailcrlf), std::begin(crlf), std::end(crlf) );

            prevpos = pos+1;
            pos = offset(vmail, "\n", pos+1);
        }
    }
    else {
        vmailcrlf = std::vector<char> (vmail.begin()+firstline, vmail.end());
    }

    if (bCompressEml) {
        std::vector<char> eml_gz = compress_gzip(vmailcrlf);
        vmailcrlf = eml_gz;
    }
}
//---------------------------------------------------------------------------------------------
/**
 *  SaveToEML()
 *  Save email to eml file with name formated as "YYYYmmddHHMMSS_MD5ofMessageID.eml"
 *  or "YYYYmmddHHMMSS_MD5ofMessageID.eml.gz" if compressed
 *  Return true if succeed
 */
bool Mbox_parser::SaveToEML(){

    string emlfullname = outputdirectory + emlfilename;
    std::ofstream f( emlfullname, std::ofstream::binary );
    if (! f.is_open()){
        if (*cbFunc_log) cbFunc_log ("ERROR", "Could not open \""+emlfullname+"\"");
        return false;
    }

    StoreEML();

    f.write(vmailcrlf.data(), vmailcrlf.size());
    if (f.bad()) {
        std::remove(emlfullname.c_str());
        if (*cbFunc_log) cbFunc_log ("ERROR", "Could not write to \""+emlfullname+"\"");
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  SaveToCompact()
 *  Add full email (with line "From - ...") to new mbox file named "mboxfilename_YYYYmmddHHMMSS"
 *  Return true if succeed
 */
bool Mbox_parser::SaveToCompact(){

    outputcompact.write(vmail.data(), vmail.size());
    if (outputcompact.bad()) {
        if (*cbFunc_log) cbFunc_log ("ERROR", "Could not write to \""+compactfilename+"\". Compact process is aborted.");
        bDisableMboxCompact = true;
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  SaveToSplit()
 *  Add full email (with line "From ...") to mbox part
 *  Return true if succeed
 */
bool Mbox_parser::SaveToSplit(){

    if (!mboxsplitmaxsize) {
        return false;
    }

    // If an email size exceed max split size
    if (mailsize > mboxsplitmaxsize){
        if (*cbFunc_log) cbFunc_log ("ERROR", "At least one email exceeds the defined maximum size of the split file. Split process is aborted.");
        bDisableMboxSplit = true;
        return false;
    }

    // if first file or add email is over maxsplit then creation of a new file
    if (!splitindex || mboxsplitcurrentsize+mailsize > mboxsplitmaxsize){

        int maxsliptcount = ceil(double(mboxlength) / double(mboxsplitmaxsize));
        int maxsplitfill = ceil(log10(fabs(maxsliptcount)+1));

        stringstream ss;
        ss << mboxfilename+".";
        ss << setw(maxsplitfill) << setfill('0') << ++splitindex;
        splitfilename = outputdirectory + ss.str();

        if (outputsplit.is_open()) outputsplit.close();

        if (FileExists(splitfilename)) std::remove(splitfilename.c_str());
        outputsplit.open( splitfilename, std::ofstream::binary | std::ofstream::app );
        if (!outputsplit.is_open()){
            if (*cbFunc_log) cbFunc_log ("ERROR", "Could not open \""+splitfilename+"\". Split process is aborted.");
            bDisableMboxSplit = true;
            return false;
        }

        mboxsplitcurrentsize = 0;
        nbsplitfile++;
    }

    // Append data to file
    outputsplit.write(vmail.data(), vmail.size());
    if (outputsplit.bad()) {
        if (*cbFunc_log) cbFunc_log ("ERROR", "Could not write to \""+splitfilename+"\". Split process is aborted.");
        outputsplit.close();
        bDisableMboxSplit = true;
        return false;
    }

    mboxsplitcurrentsize += mailsize;

    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  GetHeaderField()
 *  Read email header specified (even on multiple lines)
 *  Option 'index' could be used when there is many headers with same name (eg:'Received')
 *  Return the value of the header field else empty string
 */
string Mbox_parser::GetHeaderField(string headerField, bool insensitiveSearch, int index) {

    int idx_headerField = 0;

    headerField = "\n"+headerField; // Prepend with "\n" to be sure it is not a text contained in field's value
    headerField += ":";

    std::vector<char> vHeaderValue;
    string headerValue;

    size_t line=0;
    while (idx_headerField++ <= index) {
        if (insensitiveSearch) line = ci_offset(vheader, headerField, line);
        else line = offset(vheader, headerField, line);
        if (line==(size_t)-1) return "";
        line++;
    }
    size_t endline = offset(vheader, "\n", line+1); // +1 to ignore 1st char '\n ' of headerField
    if (endline==(size_t)-1) endline = vheader.size();

    // Test if newline is windows crlf
    if ((line+headerField.length() < endline) && vheader[endline-1] == '\r')
        vHeaderValue = std::vector<char> (vheader.begin()+line+headerField.length(), vheader.begin()+endline-1);
    // do next test length in case of empty field value (then line+headerField.length() >= endline !!!)
    else if (line+headerField.length() < endline)
        vHeaderValue = std::vector<char> (vheader.begin()+line+headerField.length(), vheader.begin()+endline);

    vHeaderValue.push_back('\0');
    headerValue += trim(vHeaderValue.data());

    // Case multiline value
    size_t endnextline = offset(vheader, "\n", endline+1);

    while (endnextline != (size_t)-1) {
        // Test if newline is windows crlf
        if ((endline+1 < endnextline) && vheader[endnextline-1] == '\r')
            vHeaderValue = std::vector<char> ( vheader.begin()+endline+1, vheader.begin()+endnextline-1 );
        else if (endline+1 < endnextline)
            vHeaderValue = std::vector<char> ( vheader.begin()+endline+1, vheader.begin()+endnextline );
        vHeaderValue.push_back('\0');

        if (match("*: *", vHeaderValue.data())) break;
        headerValue += trim(vHeaderValue.data());

        endline = endnextline;
        endnextline = offset(vheader, "\n", endline+1);
    }

    return headerValue;
}
//---------------------------------------------------------------------------------------------
/**
 *  IsValidMail()
 *  According to rfc2822 an email's header must have fields 'Date', 'From' and should have 'Message-ID'
 *  But to ensure a certain flexibility, 'Message-ID' is here not required.
 *  This function checks only the presence of 'Date' and 'From' and test if email's date is correct
 *  Return true if this fields exists.
 */
bool Mbox_parser::IsValidMail() {

    // Sometimes the header fields are in lowercase but it is rarely the case
    // so a sensitive search is done in first place because it's faster

    headerfield_date = GetHeaderField("Date");
    if (headerfield_date.empty())
        headerfield_date = GetHeaderField("Date", true);

    headerfield_from = GetHeaderField("From");
    if (headerfield_from.empty())
        headerfield_from = GetHeaderField("From", true);

    headerfield_msgid = GetHeaderField("Message-ID");
    if (headerfield_msgid.empty())
        headerfield_msgid = GetHeaderField("Message-ID", true); // sometimes "Message-Id" and not "...ID" !

    if (headerfield_date.length() && headerfield_from.length() && GetMailDate())
        return true;

    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  IsDeletedMail()
 *  Check for deletion in X-Mozilla-Status (see http://mxr.mozilla.org/mozilla/source/mailnews/base/public/nsMsgMessageFlags.h)
 *  Return true if email is marked as deleted
 */
bool Mbox_parser::IsDeletedMail() {

    int  iMozStatus;
    std::stringstream stream;

    stream << GetHeaderField("X-Mozilla-Status");
    stream >> std::hex >> iMozStatus;
    if (iMozStatus & MSG_FLAG_EXPUNGED) return true;

    stream.str(std::string()); stream.clear();
    stream << GetHeaderField("X-Mozilla-Status2");
    stream >> std::hex >> iMozStatus;
    if (iMozStatus & MSG_FLAG_IMAP_DELETED) return true;

    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  IsExcludedMail()
 *  Return true if email is excluded by the date filtering rules
 */
bool Mbox_parser::IsExcludedMail() {

    if (tt_maildateafter == tt_maildatebefore) return false;

    if (tt_maildateafter && !tt_maildatebefore) {
        // If date email older than "date after" then exclude
        if (tt_maildate <= tt_maildateafter) return true;
    }
    else if (tt_maildatebefore && !tt_maildateafter) {
        // If date email younger than "date before" then exclude
        if (tt_maildate >= tt_maildatebefore) return true;
    }
    else if (tt_maildateafter && tt_maildatebefore) {
        // Reject emails between tt_maildatebefore AND tt_maildateafter
        // tt_maildatebefore < tt_maildate < tt_maildateafter
        if (tt_maildateafter > tt_maildatebefore) {
            if (tt_maildatebefore <= tt_maildate && tt_maildate <= tt_maildateafter) return true;
        }
        // Reject emails before tt_maildateafter OR after tt_maildatebefore
        // tt_maildate < tt_maildateafter || tt_maildate > tt_maildatebefore
        else if (tt_maildateafter < tt_maildatebefore) {
            if (tt_maildate <= tt_maildateafter || tt_maildate >= tt_maildatebefore) return true;
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------------
/**
 *  GetMailAvailable()
 *  According to the options 'ExtractDeleted' and 'ExtractInvalid' available emails
 *  may contain deletes or invalids in addition to valid emails
 *  Return the number of available emails
 */
int Mbox_parser::GetMailAvailable(){

    return nbmailok;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailRead(){

    return nbmailread;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailInvalid(){

    return nbmailinvalid;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailDeleted(){

    return nbmaildeleted;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailDuplicated(){

    return nbmailduplicated;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailExcluded(){

    return nbmailexcluded;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailExtracted(){

    return nbmailextracted;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailCompact(){

    return nbmailcompact;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetMailSplit(){

    return nbmailsplit;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetSplitFile(){

    return nbsplitfile;
}
//---------------------------------------------------------------------------------------------
int Mbox_parser::GetEmlDeleted(){

    return nbemlremoved;
}
//---------------------------------------------------------------------------------------------
std::vector<string> Mbox_parser::GetEmlList(){

    return emlList;
}
//---------------------------------------------------------------------------------------------
/**
 *  SetWindowsFormat()
 *  If argument is true then convert the eml to windows format :
 *      if necessary, all the new line characters are converted to "\r\n".
 *  Else keeps the newline as it is read
 */
void Mbox_parser::SetWindowsFormat(bool b) {
    bEmlToWindows = b;
}
//---------------------------------------------------------------------------------------------
/**
 *  SetSynchronize()
 *  If argument is true then all extracted eml files that
 *  are no more in valid emails list are deleted.
 *  The vector 'emlList' then contains the names of the valid files.
 *  default is false
 */
void Mbox_parser::SetSynchronize(bool b){
    bSynchronize = b;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetActionExtract(bool b, bool compress){

    bExtractMboxEml = b;
    bCompressEml = compress;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetActionCompact(bool b){

    bGenerateMboxCompact = b;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetActionSplit(bool b, size_t maxsize){

    bGenerateMboxSplit = b;
    mboxsplitmaxsize = maxsize;
}
//---------------------------------------------------------------------------------------------
/**
 *  set mbox process output directory
 *  return reformatted output directory if necessary indented with "/"
 *  and all characters '\\' replaced by '/'
 *  and all "//" to "/"
 */
std::string Mbox_parser::SetOutputDirectory(std::string directory){

    outputdirectory = path_dusting(directory);
    if (!outputdirectory.empty() && *outputdirectory.rbegin() != '/') // or && outputdirectory.back() != '/')
        outputdirectory += '/';

    return outputdirectory;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetExtractInvalid(bool bExtract) {
    bExtractInvalid = bExtract;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetExtractDeleted(bool bExtract) {
    bExtractDeleted = bExtract;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetExtractDuplicated(bool bExtract) {
    bExtractDuplicated = bExtract;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetAgeMin(int age) {

    mailAgeMin = age;
    if (age > 0) {
        tt_maildatebefore = tt_timezero - age*24*60*60;
    }
    else tt_maildatebefore = 0;
}
//---------------------------------------------------------------------------------------------
void Mbox_parser::SetAgeMax(int age) {

    mailAgeMax = age;
    if (age > 0) {
        tt_maildateafter = tt_timezero - age*24*60*60;
    }
    else tt_maildateafter = 0;
}
//---------------------------------------------------------------------------------------------
/**
 *  SetDateBefore()
 *  Filter emails that date before specified value
 */
bool Mbox_parser::SetDateBefore(string strdate) {

    if (strdate.empty()) return false;

    tm tm_time;
    int Y,M,d,h=0,m=0,s=0;

    mailAgeMin = 0;

    int retval = sscanf(strdate.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
    if (retval<3) retval = sscanf(strdate.c_str(), "%d/%d/%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
    if (retval<3) { tt_maildatebefore = 0; return false; }

    tm_time.tm_year = Y - 1900; // Year since 1900
    tm_time.tm_mon = M - 1;     // 0-11
    tm_time.tm_mday = d;        // 1-31
    tm_time.tm_hour = h;        // 0-23
    tm_time.tm_min = m;         // 0-59
    tm_time.tm_sec = s;
    tt_maildatebefore = mktime(&tm_time);
    if (tt_maildatebefore==-1) { tt_maildatebefore = 0; return false; }
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  SetDateAfter()
 *  Filter emails that date after specified value
 */
bool Mbox_parser::SetDateAfter(string strdate) {

    if (strdate.empty()) return false;

    tm tm_time;
    int Y,M,d,h=0,m=0,s=0;

    mailAgeMax = 0;

    int retval = sscanf(strdate.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
    if (retval<3) retval = sscanf(strdate.c_str(), "%d/%d/%d %d:%d:%d", &Y, &M, &d, &h, &m, &s);
    if (retval<3) { tt_maildateafter = 0; return false; }

    tm_time.tm_year = Y - 1900; // Year since 1900
    tm_time.tm_mon = M - 1;     // 0-11
    tm_time.tm_mday = d;        // 1-31
    tm_time.tm_hour = h;        // 0-23
    tm_time.tm_min = m;         // 0-59
    tm_time.tm_sec = s;
    tt_maildateafter = mktime(&tm_time);
    if (tt_maildateafter==-1) { tt_maildateafter = 0; return false; }
    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  GetMailDate()
 *  Functions 'get_time' or 'strptime' are not used because %Z timezone is not fully implemented
 *  Mail's date is converted to localtime (timezone is included in hours and minutes)
 *  Return true if date is valid
 *  TODO : See implementation https://howardhinnant.github.io/date/date.html
 */
bool Mbox_parser::GetMailDate(bool is_forcesearch) {

    if (bmaildatestored) return true;
    vector<string> v;
    int dayindex = 1; // Current decimal day index in vector string date;

    split(headerfield_date , ' ', v, false); // eg: "Fri, 16 Nov 2012 13:16:09 -0400"
    if (!v.empty() && std::isdigit(v[0][0])) dayindex = 0; // eg: "16 Nov 2012 13:16:09 -0400"

    // If date string is malformed then try with other string formats or to find the closest date
    if (v.size() < (size_t)(4+dayindex) ||
        !is_number(v[dayindex]) ||
        get_month_num(v[dayindex+1])==-1 ||
        !is_number(v[dayindex+2]) ||
        (v[dayindex+3].find(":") == std::string::npos) ) {

        // Exit if trying to search date (not first function call)
        if (is_forcesearch) return false;

        // Else searching for the date :
        headerfield_date.clear();

        // Trying conversion by removing "-" eg: "16-Nov-2012 13:16:09 -0400"
        if (v.size()>0 && !v[0].empty()) std::replace( v[0].begin(), v[0].end(), '-', ' ');
        if (v.size()>1 && !v[1].empty()) std::replace( v[1].begin(), v[1].end(), '-', ' ');
        for (auto const& s : v) { headerfield_date += s+" "; }
        headerfield_date = trim(headerfield_date);
        GetMailDate(true);
        if (bmaildatestored) return true;

        // Trying with an extraction of the last header 'Received'
        int index=-1;
        do {
            headerfield_date = GetHeaderField("Received", false, ++index);
        }
        while ( headerfield_date.length() ) ;

        if (index>0) headerfield_date = GetHeaderField("Received", false, index-1);
        size_t pos = headerfield_date.find_last_of(";");
        if (pos != string::npos) {
            headerfield_date = trim(headerfield_date.substr(pos+1));
            GetMailDate(true);
        }
        if (bmaildatestored) return true;

        // Date is not valid then email is invalid
        return false;
    }


    string monthname = v[dayindex+1]; // Month to int
    transform(monthname.begin(), monthname.end(), monthname.begin(),(int (*)(int))tolower);
    vector<string> vTime;
    split(v[dayindex+3], ':', vTime, false);

    // Sometimes the time is only hh:mm so we correct it
    // eg: "Wed, 29 Jan 2014 14:30 +0100"
    while ( vTime.size() < 3 )
        vTime.push_back("00");

    string smailTZ;
    if (v.size()>=(size_t)(dayindex+5)) smailTZ = v[dayindex+4]; // timezone
    else smailTZ = "0000";

    int year = atoi(v[dayindex+2].c_str());
    if (year < 90 ) year += 2000;
    else if (year < 99 ) year += 1900;

    int imailTZ = atoi(smailTZ.c_str());
    int hourmailTZ = imailTZ/100;
    int minutemailTZ = imailTZ%100;

    tm_maildate.tm_mday = atoi(v[dayindex].c_str());
    tm_maildate.tm_mon = get_month_num(monthname)-1;
    tm_maildate.tm_year = year - 1900;
    tm_maildate.tm_hour = atoi(vTime[0].c_str())-hourmailTZ+hourlocalTZ;
    tm_maildate.tm_min = atoi(vTime[1].c_str())-minutemailTZ+minutelocalTZ;
    tm_maildate.tm_sec = atoi(vTime[2].c_str());
    tt_maildate = std::mktime(&tm_maildate);

    bmaildatestored = true;

    return true;
}
//---------------------------------------------------------------------------------------------
/**
 *  EmlFilename()
 *  Generate a file name formated as "YYYYmmddHHMMSS_MD5ofMessageID.eml"
 *                                or "YYYYmmddHHMMSS_MD5ofMessageID.eml.gz"
 *  or, if 'Message-ID' is empty, as "YYYYmmddHHMMSS_MD5ofEmail.eml"
 *                                or "YYYYmmddHHMMSS_MD5ofEmail.eml.gz"
 */
string Mbox_parser::EmlFilename() {

    if (!emlfilename.empty()) return emlfilename;
    string md5str;
    if (!headerfield_msgid.empty()) {
        // Generate file name based on Message-ID hash to MD5
        md5str = PrintMD5(headerfield_msgid);
    }
    else {
        // Generate file name based on MD5 content (without "From " line)
        int firstline = offset(vmail, "\n")+1;
        std::string str(vmail.begin()+firstline,vmail.end());
        md5str = PrintMD5(str);
    }

    std::stringstream ss;

    ss << tm_maildate.tm_year+1900
        << setw(2) << setfill('0') << tm_maildate.tm_mon+1
        << setw(2) << setfill('0') << tm_maildate.tm_mday
        << setw(2) << setfill('0') << tm_maildate.tm_hour
        << setw(2) << setfill('0') << tm_maildate.tm_min
        << setw(2) << setfill('0') << tm_maildate.tm_sec
        << "_" << md5str;

    ss <<".eml";
    if (bCompressEml) ss << ".gz";

    emlfilename = ss.str();
    return emlfilename;
}
//---------------------------------------------------------------------------------------------
/**
 *  Set_Callback_Eml_Preprocess()
 *  If this callback is set then this a previous test to perform the call of function cbFunc_eml
 *
 *  Callback must be declared with syntax like this :
 *    bool mycallback(string dirname, string filename)
 *  where :
 *    - dirname is output directory (outputdirectory),
 *    - filename is eml file name (emlfilename),
 */
void Mbox_parser::Set_Callback_Eml_Preprocess(callback_func_eml_preprocess_ptr ptr) {
    cbFunc_eml_preprocess = ptr;
}
//---------------------------------------------------------------------------------------------
/**
 *  Set_Callback_Eml_Process()
 *  If this callback is set then it is perform on every valid email
 *
 *  Callback must be declared with syntax like this :
 *    void mycallback(string dirname, string filename, std::vector<char> eml)
 *  where :
 *    - dirname is output directory (outputdirectory),
 *    - filename is eml file name (emlfilename),
 *    - eml is char vector contains only message (no line "From - ..." !)
 */
void Mbox_parser::Set_Callback_Eml_Process(callback_func_eml_process_ptr ptr) {
    cbFunc_eml_process = ptr;
}
//---------------------------------------------------------------------------------------------
/**
 *  Set_Callback_Log
 *  Callback must be declared with syntax like this :
 *    void mycallback(string logtype, string logmsg)
 *  where :
 *    - logtype is the the type of log - value in {"ERROR", "WARNING", "INFO", "VERBOSE1", "VERBOSE2", "VERBOSE3"}
 *    - logmsg is the message text (ex:""Output directory is undefined"")
 *  NOTA :   "VERBOSE1", "VERBOSE2", "VERBOSE3" are respectively the specific value
 *           "ERROR", "WARNING", "INFO" for each eml extracted file
 */
void Mbox_parser::Set_Callback_Log(callback_func_log_ptr ptr) {
    cbFunc_log = ptr;
}
//---------------------------------------------------------------------------------------------

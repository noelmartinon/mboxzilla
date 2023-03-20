<?php
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

/*
 *  mboxzilla server page
 */

// !!! This source need php-mcrypt extension !!!


// ** SETTINGS **
$target_dir = "backup/";
$key="_password"; // aes_key use this value to generate a 32bits key length
$maxdelay = 60; // Max seconds allow the request be accepted ( see validateDate() )


// ** FUNCTIONS AND PROCESSING **
function aes256_cbc_decrypt($key, $data, $iv) {
  $data = openssl_decrypt($data, "aes-256-cbc", $key, OPENSSL_RAW_DATA, $iv);
  return $data;
}

/**
 * Secure connection with client date so that if request is intercepted by hack then
 * it can not be valid over $maxdelay seconds
 * This feature need time sync with client as ntp !
 * Return true if request was received in the last $maxdelay seconds (upload file time is exclude)
*/
function validateDate($date)
{
  global $maxdelay;
  $d = DateTime::createFromFormat('Ymd_His', $date);
  $now = new DateTime("now");
  $interval = $now->getTimestamp() - $d->getTimestamp();
  $upload_time = time() - $_SERVER['REQUEST_TIME']; // elapse time in second for upload process
  return ($d && $d->format('Ymd_His') === $date && $interval<$upload_time+$maxdelay);
}

function rmdir_recursive($dir)
{
	$ret = true;
	$it = new RecursiveDirectoryIterator($dir, RecursiveDirectoryIterator::SKIP_DOTS);
	$files = new RecursiveIteratorIterator($it, RecursiveIteratorIterator::CHILD_FIRST);
	foreach($files as $file) {
		if ($file->isDir()){
			if (!rmdir($file->getRealPath())) $ret = false;
		} else {
			if (!unlink($file->getRealPath())) $ret = false;
		}
		if (!$ret) return false;
	}
	if (!rmdir($dir)) return false;
	return true;
}

if ( $_SERVER['REQUEST_METHOD'] == 'POST' && empty($_POST) &&
     empty($_FILES) && $_SERVER['CONTENT_LENGTH'] > 0 )
{
  $displayMaxSize = ini_get('post_max_size');

  switch ( substr($displayMaxSize,-1) )
  {
    case 'G':
      $displayMaxSize = $displayMaxSize * 1024;
    case 'M':
      $displayMaxSize = $displayMaxSize * 1024;
    case 'K':
       $displayMaxSize = $displayMaxSize * 1024;
  }

  $error = 'Posted data is too large. '.
           $_SERVER[CONTENT_LENGTH].
           ' bytes exceeds the maximum size of '.
           $displayMaxSize.' bytes.';

  echo "ERROR#".$error;
  http_response_code(403);
  exit();
}

// Some post values are required
if (!isset($_POST["token_iv"]) || !isset($_POST["token"]) ||
   (!isset($_POST["check"]) && !isset($_POST["checkfile"]) && !isset($_POST["sync_filelist"]) && !isset($_POST["sync_dirlist"]) && !isset($_POST["get_filelist"]) && !isset($_POST["iv"]))) {
	http_response_code(403);
	echo "ERROR#Remote access denied 0";
	exit();
}

$aes_key = substr(hash('sha256', $key), 0, 32);
$aes_iv_token = base64_decode($_POST["token_iv"]);
$aes_token = base64_decode($_POST["token"]);

$token = aes256_cbc_decrypt($aes_key, $aes_token, $aes_iv_token);
if (!validateDate($token)){
	echo "ERROR#Remote access denied";
	http_response_code(403);
	exit();
}

if(isset($_POST["check"])) {
	if ($_POST["check"]=="HELLO") {
		echo "READY";
		http_response_code(200);
	}
	else http_response_code(403);
	exit();
}

//// Test if file must be uploaded (=file not exist)
if(isset($_POST["checkfile"])) {
	$target_file = $target_dir . $_POST["checkfile"];
    if (!file_exists($target_file)) http_response_code(200);
    else {
		http_response_code(403);
	}
    exit();
}

if(isset($_POST["get_filelist"])) {
	$directory = $target_dir .$_POST["get_filelist"];
	if(!is_dir($directory)) {
		http_response_code(403);
		exit();
	}

	$scanned_directory = array_filter(scandir($directory), function($item) {
		global $directory;
		return is_file($directory . $item);
	});

	echo gzencode(json_encode(array_values($scanned_directory)),9);
	exit();
}

/*
 *  Sync email files (eml or eml.gz) - delete emails that client does not have
 */
if(isset($_POST["sync_filelist"]) && isset($_POST["sync_directory"])) {
	$retval = true;
	$deleted_ok = 0;
	$deleted_err = 0;
	$eml_valid = json_decode(gzdecode(base64_decode($_POST["sync_filelist"])), true);
	$directory = $target_dir .$_POST["sync_directory"];

	if(!is_dir($directory)) {
		echo "INFO#-> Nothing to do\n";
		exit();
	}

	// Set array to avoid php warning on array_diff() with an empty array argument
	if (empty($eml_valid)) $eml_valid = array("");

	$scanned_directory = array_filter(scandir($directory), function($item) {
		global $directory;
		return is_file($directory . $item);
	});

	$emltoremove = array_diff($scanned_directory, $eml_valid);

	// Remove unnecessary email files
	foreach ($emltoremove as $eml) {
		if (unlink($directory.$eml)) {
			echo "VERBOSE3#-> Successfully deleted \"".$eml."\"\n";
			$deleted_ok++;
		}
		else {
			echo "VERBOSE1#-> Unable to remove \"".$eml."\"\n";
			$retval = false;
			$deleted_err++;
		}
	}

	// Remove empty dir
	if (count(scandir($directory)) == 2) {
		if (rmdir($directory)) echo "VERBOSE3#-> Successfully deleted \"".$directory."\"\n";
		else {
			echo "VERBOSE1#-> Unable to remove \"".$directory."\"\n";
			$retval = false;
		}
	}

	if (!$deleted_ok && !$deleted_err)
		echo "INFO#-> Nothing to do (".count($scanned_directory)." emails on server)\n";
	else
		echo "INFO#-> $deleted_ok deletions succeed and $deleted_err in failure\n";
	if (!$retval) http_response_code(403);
    exit();
}

/*
 *  Sync directory tree - delete directories that client does not have
 */
if(isset($_POST["sync_dirlist"]) && isset($_POST["sync_directory"])) {
	$retval = true;
	$deleted_ok = 0;
	$deleted_err = 0;
	$dir_valid = json_decode(gzdecode(base64_decode($_POST["sync_dirlist"])), true);
	$directory = $target_dir .$_POST["sync_directory"];

	$iter = new RecursiveIteratorIterator(
		new RecursiveDirectoryIterator($directory, RecursiveDirectoryIterator::SKIP_DOTS),
		RecursiveIteratorIterator::SELF_FIRST,
		RecursiveIteratorIterator::CATCH_GET_CHILD // Ignore "Permission denied"
	);

	$local_dirlist = array();
	foreach ($iter as $path => $dir) {
		if ($dir->isDir()) {
			$local_dirlist[] = substr($path, strlen($target_dir))."/"; // adding "/" because mboxzilla directories list ending like that
		}
	}

	$dirtoremove = array_diff($local_dirlist, $dir_valid);

	function is_forbidden($forbiddennames, $stringtocheck)
	{
		foreach ($forbiddennames as $name) {
			if (stripos($name, $stringtocheck) !== FALSE) {
				return true;
			}
		}
	}

	// Sort in reverse order to recursively remove directories
	rsort($dirtoremove);
	foreach ($dirtoremove as $key => $val) {
		// Keep value only if not a parent directory of elements found in "sync_directory" list
		if (!is_forbidden($dir_valid, $val)) {
			$retval = false;
			$dir = $target_dir .$val;
			if (rmdir_recursive($dir))
				echo "VERBOSE3#-> Successfully deleted \"$dir\"\n";
			else
				echo "VERBOSE1#-> Unable to remove \"$dir\"\n";
		}
	}

	// Directories was the same
	if ($retval) echo "INFO#-> Nothing to do\n";

    exit();
}

if(!isset($_POST["iv"])) {
	http_response_code(403);
	exit();
}

if (!isset($_FILES["fileToUpload"])) exit();

// Decrypt uploded file
$aes_iv = base64_decode($_POST["iv"]);
$filename = $_FILES["fileToUpload"]["tmp_name"];
$contents = file_get_contents ($filename);
$contents = aes256_cbc_decrypt($aes_key, $contents, $aes_iv);
file_put_contents ($filename, $contents);

// Get final file name
$fullpath = base64_decode($_FILES["fileToUpload"]["name"]);
$target_file = $target_dir . $fullpath;
$uploadOk = 1;

// Check if file already exists
if (file_exists($target_file)) {
	http_response_code(403);
    echo "Sorry, file already exists.";
}

// Check if $uploadOk is set to 0 by an error
if ($uploadOk == 0) {
    echo "Sorry, your file was not uploaded.";
    http_response_code(403);
// if everything is ok, try to upload file
} else {
    if(!is_dir(dirname($target_file)))
		if (!mkdir(dirname($target_file), 0777, true)) {
		   die("Unable to create directory.");
		}
    if (move_uploaded_file($_FILES["fileToUpload"]["tmp_name"], $target_file)) {
        echo "VERBOSE3#Successfully uploaded ". basename($target_file);
    } else {
        http_response_code(403);
        echo "VERBOSE1#Failed to upload ". basename($target_file);
    }
}
?>


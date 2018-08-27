#! /usr/bin/env python3

"""
ParseAdvisory.py

Maintainer: keibarre@REDACTED.com (Keith Barrett)

NOTE: This code has been scrubbed of confidential information and is non-functional.

Parses auto-generated defect advisory text file(s) and creates csv file(s) for loading into
a spreadsheet or SQL database.

Sample Usage:

./ParseAdvisory.py file1.txt,file2.txt                  (creates file1.txt.csv and file2.txt.csv)
./ParseAdvisory.py file1.txt --output file1.csv         (creates file1.csv from file1.txt)
./ParseAdvisory.py file1.txt,file2.txt --output AllFiles.csv --append
                                                        (converts file1.txt and file2.txt into a single csv)
ls *.txt | ../ParseAdvisory.py --output                 (process all matching txt files and creates individual
                                                         csv for each)
ls *.txt | ../ParseAdvisory.py --output AllFiles.csv --append
                                                        (process all matching txt files and creates a single csv)

If required arguments are missing, the program will ask for them.

"--help" lists the possible arguments.

"""

from __future__ import print_function

import sys
import os
import argparse
import re
import csv
import platform
import requests
import json

#
# Constants and Globals
#

VERSION = "-1.0"  # Version of this program
DEBUG_MODE = False


#
# -- Classes and Objects
#

class AdvisoryObject(object):
    """ Data Object for manipulating and printing the parsed Advisory line."""

    def csv_header(self):
        """ Returns the csv column headers for this object."""
        return iter(self.header)
        # Return column headers for CSV writer

    def csv_short_string(self):
        """ Returns a csv compatible short string for the advisory data odvisory data object."""
        return "\"" + self.file_name + \
               "\",\"" + self.defect_id + \
               "\",\"" + self.sw_product_name + \
               "\",\"" + self.patched_release + \
               "\",\"" + self.patched_status + "\""

    def clear_defect(self):
        """Clear the entire defect block (defect ID, Product & release data) from the object"""
        self.defect_id = ''
        self.sw_product_name = ''
        self.sw_release_str = ''
        self.patched_release = ''
        self.patched_status = 'NOT CHECKED'
        return 0

    def clear_release(self):
        """ Clear just the release (line) data from the object."""
        self.sw_release_str = ''
        self.patched_release = ''
        self.patched_status = 'NOT CHECKED'
        return 0

    def __init__(self):
        self.file_name = ""
        self.advisory_name = ""
        self.advisory_id = ""
        self.defect_id = ""
        self.sw_product_name = ""
        self.sw_release_str = ""
        self.patched_release = ""
        self.patched_status = 'NOT CHECKED'

        self.header = ["AdvisoryFile", "AdvisoryName", "AdvisoryID",
                       "DefectID", "ProductName", "Version", "PatchedRelease", "patchedStatus"]

    def __iter__(self):
        self.list = [self.file_name, self.advisory_name, self.advisory_id, self.defect_id,
                     self.sw_product_name, self.sw_release_str, self.patched_release, self.patched_status]
        return iter(self.list)
        # Return iteration for CSV writer

    def __str__(self):
        return ','.join(map('"{0}"'.format, self))
    # Return a string for logging and debugging


#
# -- Functions and methods --
#

def get_patched_status(defect_id, product_name, product_release, url=None):
    """Get the aggregate patched status for a defect+release+product.
    Note: The first 3 arguments are required for an accurate response. If any of them are "", this
    function will just return the first aggregate status seen in the response.

    :param defect_id: defect to look up
    :type defect_id: str

    :param product_name: product within defect_id
    :type product_name: str

    :param product_release: Release within product
    :type product_release: str

    :param url: [optional] URL to use if not the defined default
    :return: Patched Status, or "URL CALL FAILED", "NO STATUS"
    """

    dflt_url = 'http://service-json-lookup-url:port/{id}'

    def user_id():
        """ Get your user id from the os regardless of whether you are on windows or unix."""

        if platform.system() != 'Windows':
            # noinspection PyUnresolvedReferences
            return pwd.getpwuid(os.getuid()).pw_name
        else:
            # noinspection PyUnresolvedReferences
            return os.getenv('USERNAME')

    def value_compare(item1, item2):
        """Case insensitive compare that tolerates foreign characters, non-strings and nulls
        Note: This is a simplified version of nocase_compare() that does not support unicode.

        :param item1: firsat item to compare (any datatype)
        :param item2: second item to compare (any datatype)
        """

        if (item1 is not None) and (item2 is not None):
            if item1 == item2:  # This should work for any data types that are equal in content
                return True
            else:
                if type(item1).__name__ == 'str' and type(item2).__name__ == 'str':
                    return item1.upper().lower() == item2.upper().lower()
                    # This little trick slightly handles forgeign characters beter
        return False

    def clean_value(data_item, def_value):
        """Return a data item, or a default value if the data item is null or bad data

        :param data_item: Data item to check and return if OK
        :param def_value: Data to return if data_item is missing, invalid or null
        """

        if data_item is None:
            return def_value
        else:
            if data_item == 'No Match Row Id' or value_compare(data_item, 'null'):
                return def_value
            else:
                return data_item

    #
    # get_patched_status logic start
    #

    if not url:
        url = dflt_url

    if '{id}' not in url:
        url += '{id}'

    url += "?product={prod}&version={ver}"
    headers = {'user': user_id()}
    url = url.format(id=defect_id, prod=product_name, ver=product_release)
    response_str = ''
    json_data = ''

    # noinspection PyBroadException
    try:
        response_str = requests.get(url, headers=headers).text + '\n'
        json_data = json.loads(response_str)  # Parse JSON string to Python object
        # time.sleep(SLEEP_TIME)  # Sleep a bit to prevent load on the service.
    except Exception:
        sys.stderr.write('Caught {e!r}'.format(**locals()))
        return "URL CALL FAILED"

    if defect_id not in response_str:
        print("Specified defect was not in JSON response?\n")
        print(response_str + '\n')
        return "URL CALL FAILED"
        # The response does not appear to be for a defect?

    for i, item_i in enumerate(json_data['pdsl']):
        product = clean_value(json_data['pdsl'][i]['product'], '')

        if product == product_name:
            for j, item_j in enumerate(json_data['pdsl'][i]['vdsl']):
                patch_status = clean_value(json_data['pdsl'][i]
                                           ['vdsl'][j]['patchstatus'], 'COULD NOT CHECK')
                version = clean_value(json_data['pdsl'][i]['vdsl'][j]['version'], '')
                if version == product_release:
                    return patch_status

    return "NO STATUS"


def input_is_redirected():
    """Is our input redirected from a file or pipe?"""

    def are_we_pycharm():
        """Are we running under PyCharm?"""
        return_status = False
        # noinspection PyBroadException
        try:
            if os.environ['PYCHARM_HOSTED'] == '1':
                return_status = True
        except Exception:
            pass
        return return_status

    if not sys.stdin.isatty() and not are_we_pycharm():
        # PyCharm fails sys.stdin.isatty() check, so we to check that also.
        return True
    else:
        return False


def DEBUG_LOG(log_line):
    """Simple function to display debug text depending on a debug mode setting.

    :param log_line: If True/False (bool) value, turns on/off debug mode. If (int) value,
        sets logging level to that value. All else gets logged as a string.
    :type log_line: String, Bool or Int
    :return: Always returns success
    """

    global DEBUG_MODE

    # If we don't get a string, then set our log level to the value
    if type(log_line).__name__ == 'bool':
        if log_line:
            DEBUG_MODE = 1
        else:
            DEBUG_MODE = 0
        return 0

    if type(log_line).__name__ == 'int':
        # TODO Add Support For logging levels
        if log_line > 0:
            DEBUG_MODE = True
        else:
            DEBUG_MODE = False
        return 0

    if type(log_line).__name__ != 'str':
        my_log_line = str(log_line)
    else:
        my_log_line = log_line + " "

    if (DEBUG_MODE > 0) and (len(my_log_line.strip()) > 0):
        if my_log_line[0] == '\n':
            print("\nDEBUG: " + my_log_line[1:])
        else:
            print("DEBUG: " + my_log_line)

    return 0


def find_release_str_end(line_string):
    """ Finds the end of a release number in a possible mulltiple release string.

    :param line_string: String to check
    :type line_string: str
    :return: character index
    """

    tmp_pos = line_string.find(' ')

    if line_string[tmp_pos + 1] == '/':
        tmp_pos2 = tmp_pos + 3
        tmp_pos = line_string.find(' ', tmp_pos2)

    return tmp_pos


def get_params():
    """Get program parameters via command line arguments and/or interactive prompt.

    :return: parser args
    """

    parser = argparse.ArgumentParser(
        prog="ParseAdvisory",
        description="Convert Advisory text file data into a CVS",
        allow_abbrev=False,
        epilog="Program accepts piped/redirected input, but a file list specified as an argument overrides it.")

    parser.add_argument("inputFile", nargs='?', default="",
                        help="Comma separated list of advisory files parse")
    parser.add_argument("--output", "--outfile", "-o", action='store', nargs='?',
                        default="<None>", const="", dest="ofile",
                        help="Create a CSV file. If no filename is specificed it defaults to the input file "
                             "name with a \'.csv\' added.")
    parser.add_argument("--append", action="store_true", default=False,
                        help="Appends to the end of existing file instead of over-writing it")
    parser.add_argument("--checkpatched", action="store_true", default=False,
                        help="Include Patch Status")
    parser.add_argument("--debug", action="store_true", default=False,
                        help="Enables debug logging")

    my_args = parser.parse_args()

    if my_args.inputFile == "":
        try:
            if input_is_redirected():  # Read from pipe or file
                for redirectedInput in sys.stdin:
                    my_args.inputFile += redirectedInput.strip() + ','
                my_args.inputFile = my_args.inputFile[:-1]
        except NameError:
            pass

    if my_args.inputFile == "":
        my_args.inputFile = input("Advisory Files(s)?: ")  # Prompt
        # If defect not specified, prompt for it

    if my_args.inputFile == "":
        print("\nNo Advisory filenames were provided!\n")
        parser.print_help(sys.stderr)
        sys.exit(1)

    return my_args


def cleaned_up(line_str):
    """Strip off binary conversion envelope if there.

    :param line_str: String to parse
    :type line_str: str
    :return: cleaned up string
    """
    my_line_str = str(line_str)
    if (my_line_str.startswith("b\'") or my_line_str.startswith("B\'")) \
            and my_line_str.endswith("\\n\'"):
        my_line_str = my_line_str[2:-3]

        if my_line_str.endswith("\\r"):
            my_line_str = my_line_str[:-2]

    my_line_str = re.sub('\s+', ' ', my_line_str).strip('\r').strip('\n').strip()
    # Collapse whitespaces and remove terminators

    return my_line_str


def write_parsed_lines(advisory_obj, writer=None, check_patched_status=False):
    """Writes individual lines if there are two releases or defects (nn/nn) in one field.

    :param advisory_obj: The advisory object to process/write
    :type advisory_obj: AdvisoryObject
    :param writer: The CSV writer to use. If None, then the function
    prints the object short string on screen.
    :type writer: csv.writer
    :param check_patched_status: Do we check the patch status?
    :type check_patched_status: Bool
    :return: The number of lines produced/written
    """

    old_sw_release_str = advisory_obj.sw_release_str
    old_patched_release = advisory_obj.patched_release
    old_defect = advisory_obj.defect_id

    version = advisory_obj.sw_release_str.split('/')
    defect = advisory_obj.defect_id.split('/')

    if advisory_obj.patched_release == "N/A":
        patched_release = advisory_obj.patched_release.split('-')
    else:
        patched_release = advisory_obj.patched_release.split('/')

    my_lines_written = 0
    my_defect_index = 0

    while my_defect_index < len(defect):

        advisory_obj.defect_id = defect[my_defect_index]
        my_index = 0

        while my_index < max(len(version), len(patched_release)):

            if my_index < len(version):
                advisory_obj.sw_release_str = version[my_index]
            else:
                advisory_obj.sw_release_str = version[0]

            if my_index < len(patched_release):
                advisory_obj.patched_release = patched_release[my_index]
            else:
                advisory_obj.patched_release = patched_release[0]

            if check_patched_status:
                advisory_obj.patched_status = get_patched_status(
                    advisory_obj.defect_id,
                    advisory_obj.sw_product_name,
                    advisory_obj.patched_release)
            else:
                advisory_obj.patched_status = "NOT CHECKED"
                # Check for patch status

            if writer:
                DEBUG_LOG("Writing " + str(advisory_obj))
                writer.writerow(advisory_obj)

            my_lines_written += 1
            my_index += 1

        my_defect_index += 1

    if my_lines_written > 1:
        advisory_obj.sw_release_str = old_sw_release_str
        advisory_obj.patched_release = old_patched_release
        advisory_obj.defect_id = old_defect
        advisory_obj.patched_status = ''
        # Put the object items back to original values if the fields held multiple values

    return my_lines_written


def load_advisory_file(input_file):
    """Load advisory file lines into list for processing.

    :param input_file: file to process
    :type input_file: Str
    :return: string list; one element per line loaded
    """

    advisory_lines = []
    del advisory_lines[:]
    skip_lines = True  # Start off skipping blocks of text we have no interest in
    lines_read = 0

    # noinspection PyBroadException
    try:
        my_file_handle = open(input_file, 'rb')
        # Have to open as binary, then read/parse into a valid string

        with my_file_handle:
            for lines_read, source in enumerate(my_file_handle):
                line_str = cleaned_up(source)
                lower_line_string = line_str.lower()

                if ('n the following tables' in lower_line_string) or (lower_line_string[0:11] == '[defect-aler'):
                    skip_lines = False

                if not skip_lines:
                    advisory_lines.append(line_str)

                if (lower_line_string[0:12] == 'bug id') or ('public announcements' in lower_line_string):
                    skip_lines = True  # Turn it off after logging the line we saw (easier to debug)

        my_file_handle.close()
    except Exception:
        pass

    if input_file:
        DEBUG_LOG("Total of " + str(lines_read) + " read from file \'"
                  + input_file + "\' in load_advisory_file()")

    return advisory_lines


def set_product_name(current_line, next_line, advisory_obj):
    """Check and set advisory_obj.sw_product_name

    :param current_line: the text from the current line in advisory_lines
    :param next_line: The text from the next line in advisory_lines (or "") for look ahead
    :param advisory_obj: the advisory object to check/set
    :return: True/False did we change it?
    """

    if (advisory_obj.defect_id != '') and ('first fixed release' in current_line.lower()) \
            and (current_line[0:6].lower() == 'My Company ') and (advisory_obj.sw_product_name == ''):

        # We are either on the product name line, or it's the next line
        tmp_line = current_line[6:]

        if tmp_line[0:6].lower() == "first ":
            tmp_line = next_line
            # It was on the next line

        advisory_obj.sw_product_name = tmp_line.split(' ')[0]
        return True
    else:
        return False


def set_defect(current_line, previous_line, advisory_obj):
    """Check and set advisory_obj.product line and advisory_obj.defectId

    :param current_line: the text from the current line in advisory_lines
    :param previous_line: The text from the previous line in advisory_lines (or "") for look back.
    :param advisory_obj: the advisory object to check/set
    :return: True/False did we change it?
    """

    combined_str = previous_line + ' ' + current_line

    if (": INC" in combined_str) and \
            ('bug id' not in combined_str.lower()) and (current_line != ''):

        advisory_obj.clear_defect()  # Clear (any) previous defect block

        tmp = combined_str.find(': ')
        advisory_obj.product_line = combined_str[:tmp].strip()
        skip_product_line = False

        for i in advisory_obj.skip_products:
            if (' ' + i + ' ' in advisory_obj.product_line) or \
                    advisory_obj.product_line.startswith('' + i + ' '):
                skip_product_line = True

        if skip_product_line:
            sys.stderr.write("\nNotice: Skipped Product Line \'" + advisory_obj.product_line + "\'\n")
            advisory_obj.clear_defect()
        else:
            advisory_obj.defect_id = combined_str[tmp + 2:].replace(' and ', '/')
            # Set advisory Defect ID (can be 2 on one line)

        return True
    else:
        return False


def skip_release_block(current_line, advisory_obj):
    """See if this release block needs to be skipped or processed

    :param current_line: the text from the current line in advisory_lines
    :param advisory_obj: the advisory object to check/set
    :return: True/False did we change it?
    """

    return_status = False
    cur_lower_line_str = current_line.lower()

    if current_line == '':  # Flag block is finished if we encounter a blank line
        advisory_obj.clear_defect()
        return_status = True

    if len(current_line.split(' ')[:]) < 3:
        # Skip standalone version lines we've already processed
        return_status = True

    if ('not vulnerable' in cur_lower_line_str) or ('not affected' in cur_lower_line_str):
        # Skip releases not impacted (why are they included?)
        return_status = True

    if 'no fix available' in cur_lower_line_str:
        # Skip releases with no fixes
        return_status = True

    if current_line[1:7] == 'rior to':  # Product release is on multiple lines
        # Skip 'Prior to' lines; they don't have a specific release to check
        return_status = True

    if current_line[0:1] not in '0123456789':
        # We're not on a release number line
        return_status = True

    return return_status


def set_release_line(current_line, advisory_obj):
    """set the three advisory_obj() release numbers contained in this current_line

    :param current_line: the text from the current line in advisory_lines
    :param advisory_obj: the advisory object to check/set
    """
    tmp_line = current_line.replace(' or later', '')
    tmp_line = tmp_line.replace(' or ', '/')
    tmp_line = tmp_line.replace(' and ', '/')
    tmp_line = tmp_line.replace(' / ', '/')
    # Compress down or remove any text or multiple releases on this line
    # The order of the above lines is significant, as they can eat each other

    advisory_obj.sw_release_str = tmp_line.split(' ')[0]
    advisory_obj.patched_release = tmp_line.split(' ')[1]

    if advisory_obj.patched_release.lower() in 'affected; migrate':
        advisory_obj.patched_release = advisory_obj.patched_release + ' (migrate)'
        # If First Fixed says migrate instead of a release, used first fixed in all field.


def main():
    """ParseAdvisory main()"""

    suppress_header = False
    open_mode = 'w'
    files_processed = 0
    output_csv = False
    lines_written = 0
    writer = ''
    append_mode = False
    check_patched_status = False

    print("\n" + "Parse Advisory File(s) - Version " + VERSION + "\n")

    #
    # Parse arguments
    #

    args = get_params()

    input_file_list = str(args.inputFile)
    display_short_line = args.display

    if args.ofile != "<None>":
        output_csv = True
        output_file = args.ofile
    else:
        output_file = ""

    if output_file == "":
        output_file_specified = False
    else:
        output_file_specified = True

    if args.debug:
        DEBUG_LOG(args.debug)

    if args.checkpatched:
        check_patched_status = args.checkpatched

    if args.append:
        suppress_header = True
        append_mode = True
        open_mode = 'a'

    if (not output_csv) and (not display_short_line):
        print("Error: Must specify either --display or --output, or both")
        sys.exit(1)

    DEBUG_LOG("Files to parse: \"" + input_file_list + "\"")

    if output_file_specified:
        DEBUG_LOG("Output File: \"" + output_file + "\"")

    cur_output_file = output_file

    #
    # Loop through comma separated list of advisory files and select one to process
    #

    for input_file in input_file_list.split(','):
        print("Processing file \'" + input_file + "\'...")

        advisory_obj = AdvisoryObject()
        advisory_obj.file_name = os.path.basename(input_file)

        output_handle = ''

        if output_csv:
            if not output_file_specified:
                cur_output_file = advisory_obj.file_name + '.csv'
                DEBUG_LOG("Output File: \"" + cur_output_file + "\"")
                lines_written = 0

            output_handle = open(cur_output_file, open_mode, newline='')
            writer = csv.writer(output_handle, delimiter=',', quotechar='"', quoting=csv.QUOTE_ALL)

            if not suppress_header:
                writer.writerow(advisory_obj.csv_header())
                lines_written += 1

            if output_file_specified:
                open_mode = 'a'
                suppress_header = True
                # A single output from multiple inputs becomes an operational append after 1st input file

        #
        # Load this advisory
        #

        advisory_lines = load_advisory_file(input_file)
        DEBUG_LOG("Total of " + str(len(advisory_lines)) + " lines loaded for parsing")
        # Load the whole file into an array. This will allow us to look forward or back 1 line at need

        #
        # Loop through the text lines in this advisory and build/write advisory_obj lines
        #

        for index, cur_line_str in enumerate(advisory_lines):

            if index > 0:
                previous_line_str = advisory_lines[index - 1]
            else:
                previous_line_str = ""

            if index < len(advisory_lines) - 1:
                next_line_str = advisory_lines[index + 1]
            else:
                next_line_str = ""

            cur_lower_line_str = cur_line_str.lower()

            # Advisory ID (one per file)
            if (cur_line_str == 'Advisory ID:') and (advisory_obj.advisory_id == ''):
                advisory_obj.advisory_id = next_line_str
                DEBUG_LOG("Setting Advisory ID to \'" + advisory_obj.advisory_id + "\'")
                continue

            # Advisory Name (one per file)
            if (cur_lower_line_str[0:11] == "[defect-aler") and (advisory_obj.advisory_name == ''):
                advisory_obj.advisory_name = advisory_lines[index + 2] + ' ' + advisory_lines[index + 3]
                DEBUG_LOG("Setting Advisory Name to \'" + advisory_obj.advisory_name + "\'")
                continue

            # Defect ID (one per defect block)
            if set_defect(cur_line_str, previous_line_str, advisory_obj):
                if advisory_obj.defect_id != '':
                    DEBUG_LOG("Now on Defect \'" + advisory_obj.defect_id + "\'")
                continue

            # SW Product Name (one per defect block)
            if set_product_name(cur_line_str, next_line_str, advisory_obj):
                DEBUG_LOG("Set SW ProductName to \'" + advisory_obj.sw_product_name + "\'")
                continue

            # SW Product release, first fixed release, and first fix for all (one per row)
            if (advisory_obj.sw_product_name != '') and ('ulnerabilit' not in cur_line_str):

                # We're inside a release block

                if skip_release_block(cur_line_str, advisory_obj):
                    # Do we need to process this block?
                    continue
                else:
                    # We're inside the release lines in a release block
                    set_release_line(cur_line_str, advisory_obj)

                if output_csv:
                    lines_written += write_parsed_lines(advisory_obj, writer, check_patched_status)
                else:
                    if display_short_line:
                        write_parsed_lines(advisory_obj, None, check_patched_status)

                advisory_obj.clear_release()
                # Prevent accidental hold over of values since these are per row

        if output_csv:
            output_handle.close()
            if not output_file_specified:
                print()
                print("Total of " + str(lines_written) + " lines writen to \'" + cur_output_file + "\'\n")

        files_processed += 1

    print()

    if files_processed > 1:
        print("Total of " + str(files_processed) + " files processed\n")

    if output_csv and output_file_specified:
        if append_mode:
            print("Grand total of " + str(lines_written) + " lines APPENDED to \'" + cur_output_file + "\'\n")
        else:
            print("Grand total of " + str(lines_written) + " lines written to \'" + cur_output_file + "\'\n")

    return 0


#
# main() Entry Point
#

if __name__ == '__main__':
    main()


package com.dti.log.monitor;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Date;

import org.apache.log4j.Logger;

import com.dti.common.LogMsg;
// TODO all debug statements should reference this.class()
// TODO turn debug on/off from config file

/**
 * Creates a list of filter rules loaded from the rules text file, and
 * compares an alert against the rule for possible special processing
 * or skipping.
 * 
 * Rewritten Nov 2006 by Keith Barrett
  * <br>
 * Last update: 12/30/2007 Keith Barrett  RAD7 and PMD corrections
 * 
 * @author barrk012
 * @version 1.1
 */

public class AlertFilter {

	/** Rule filename */
	public static final String FILTER_FILENAME = "LogMonitorFilters.txt";

	private static final int MAX_FILTERS = 300;

	final private static String SEPARATOR = "|";

	/** Our Logger */
	private static Logger log = null;

	/** Filter list */
//	private static FilterRules[] rules = null;

	/** Filter count */
	private static int numberOfRules = 0;

	/** Number of rules found in the last filter file load */
	private static int oldNumberOfRules = 0;

	/** Version number of last filter file load */
	private static String filterFileVersion = "0";

	public AlertFilter() {
		new LogMsg();
		log = LogMsg.getLoggerID();
	}


	/**
	 * Verifies and populates a filter rules
	 * @param rule The rule object to load
	 * @param ruleNbr the number of this rule
	 * @param lineNbr the line number in the file for this rule
	 * @param filterText the "Filter:" text description of this rule
	 * @param ruleTextLine the rule to parse
	 * @return true or false; did this work?
	 */
	private boolean loadFilter(
		final FilterRules rule,
		final int ruleNbr,
		final int lineNbr,
		final String filterText,
		final String ruleTextLine) {
		boolean result = true;
		String line = ruleTextLine;
		int startIndex = 0;
		int endIndex = 0;
		int fieldNumber = 0;

		LogMsg.debug(log, "-- Entered loadFilter() --");

		if ((line == null) || (rule == null)) {
			result = false;
		} else {
			line = line.trim() + SEPARATOR;
			// Ensure we don't walk off the end of a properly formatted line
		}

		while ((result == true)
			&& (startIndex >= 0)
			&& (fieldNumber < FilterRules.FIELD_COUNT)) {

			++fieldNumber;
			endIndex = line.indexOf(SEPARATOR, startIndex + 1);

			String field = line.substring(startIndex, endIndex).trim();
			startIndex = endIndex + 1;

			LogMsg.debug(
				log,
				"Processing field #"
					+ fieldNumber
					+ ", data = \""
					+ field
					+ "\"");


[CODE REMOVED]

	/**
	 * Resets internal filter counts back to zero, and re-load the rules from the
	 * file.
	 */
	public FilterRules[] resetFilters() {
		String ruleFileLine = null;
		boolean fileOpen = false;
		int lineCount = 0;
		boolean goAhead = true;
		BufferedReader tempBr = null;
		String currentText = FilterRules.DFLT_FILTER_TEXT;
		String fileVersion = "0";

		LogMsg.debug(log, "-- Entered resetFilters() --");

		numberOfRules = 0;
		FilterRules[] rules = new FilterRules[MAX_FILTERS];

		LogMsg.debug(log, "Opening \"" + FILTER_FILENAME + "\" ...");

		try {
			final FileInputStream fis = new FileInputStream(FILTER_FILENAME);
			tempBr = new BufferedReader(new InputStreamReader(fis));
			fileOpen = true;

			while (((ruleFileLine = tempBr.readLine()) != null)
				&& (numberOfRules < MAX_FILTERS)
				&& (goAhead)) {

				ruleFileLine = ruleFileLine.trim();
				++lineCount;

				if (ruleFileLine.equals("")) {
					goAhead = false;
					LogMsg.debug(
						log,
						"Skipping blank line #" + lineCount + " in file");
				} else if (ruleFileLine.substring(0, 1).equals("#")) {
					goAhead = false;
					LogMsg.debug(
						log,
						"Skipping comment at line #" + lineCount + " in file");
				} else if (
					ruleFileLine.substring(0, 7).equalsIgnoreCase("Filter:")) {

					final int len = ruleFileLine.length();
					currentText = ruleFileLine.substring(7, len).trim();
					LogMsg.debug(
						log,
						"Setting filterText to \"" + currentText + "\"");
					goAhead = false;
					// memorize the last filter text we encountered

				} else if (
					ruleFileLine.substring(0, 8).equalsIgnoreCase(
						"Version:")) {

					final int len = ruleFileLine.length();
					fileVersion = ruleFileLine.substring(8, len).trim();
					LogMsg.debug(
						log,
						"Setting fileVersion to \"" + fileVersion + "\"");
					goAhead = false;
					// memorize the last filter text we encountered
				}

				if (goAhead) {
					// This is a rule to load!

					if (rules[numberOfRules] == null) {
						rules[numberOfRules] = new FilterRules();
					}

					if (loadFilter(rules[numberOfRules],
						numberOfRules,
						lineCount,
						currentText,
						ruleFileLine)) {

						++numberOfRules;
					} else {
						LogMsg.warning(
							log,
							"Skipping invalid filter definition at line "
								+ lineCount
								+ " in "
								+ FILTER_FILENAME);
					}
				} else {
					goAhead = true;
					// Skip this line; but assume we should do the next
				}
			}
		} catch (FileNotFoundException e1) {
			LogMsg.logException(
				log,
				FILTER_FILENAME
					+ " file not found; no exception filters are in effect",
				e1);
			goAhead = false;
		} catch (IOException e1) {
			LogMsg.logException(
				log,
				"I/O read from " + FILTER_FILENAME + " failed",
				e1);
			goAhead = false;
		}

		if ((numberOfRules != oldNumberOfRules)
			|| (fileVersion.compareToIgnoreCase(filterFileVersion) != 0)) {

			// If it looks like the file changed; report it
			LogMsg.info(
				log,
				""
					+ new Date().toString()
					+ " Loaded new rules; "
					+ numberOfRules
					+ " rules loaded from "
					+ FILTER_FILENAME);

			oldNumberOfRules = numberOfRules;
			filterFileVersion = fileVersion;
		}

		if (fileOpen) {
			LogMsg.debug(log, "Closing" + FILTER_FILENAME);
			try {
				tempBr.close();
			} catch (IOException e) { // ignore it
			}
		}

		LogMsg.debug(log, "-- Exiting resetFilters() --");
		return rules;
	}

	/**
	 * Determines whether this error qualifies as something we DO NOT want
	 * to be paged about. In other words; this is the exception logic.
	 * All fields are required and cannot be null
	 * 
	 * @param rules[] The arrayof  filter rule objects to check against
	 * @param msgString Formatted alert message to send
	 * @param tsmac TSMAC String
	 * @param tslcoation TSLocation
	 * @param command DTI command (Q, R, U, V, C)
	 * @param provider Target provider string
	 * @param errorDTI error value; cannot be <= 0
	 * @param errorText error text for errorDTI
	 * @param errorP error value from provider (TODO errorP currently not used)
	 * @param its_log ITS_LOG number (only used in X alerts)
	 * @return SEND_NONE, SEND_EMAIL, SEND_PAGE, SEND_BOTH
	 */
	public int check(
		final FilterRules[] rules,
		final String tsmac,
		final String tslocation,
		final String command,
		final String provider,
		final int errorDTI,
		final String errorText,
		final int errorP,
		final int its_log) {

		int result = AlertGlobals.SEND_BOTH;
		boolean abort = false;
		int ruleNbr = 0;
		boolean matched = false;
		int fieldsMatched = 0;

		LogMsg.debug(log, "-- Entered check() --");

		if ((tsmac == null)
			|| (tslocation == null)
			|| (provider == null)
			|| (command == null)
			|| (errorDTI < 0)) {

			LogMsg.error(log, "Bad data passed to AlertFilter.check()");
			abort = true;
		} else {
			LogMsg.debug(
				log,
				"Checking TSMAC=\""
					+ tsmac
					+ "\", TSLocation=\""
					+ tslocation
					+ "\", Command="
					+ command
					+ ", Provider=\""
					+ provider
					+ "\", ErrorDTI="
					+ errorDTI
					+ ", ErrorP="
					+ errorP
					+ ", ITS="
					+ its_log);
		}

		while (!matched && !abort && ruleNbr < numberOfRules) {
			FilterRules filterRule = rules[ruleNbr];
			fieldsMatched = 0;

[CODE REMOVED]



	/**
	* FilterRules data object
	*/
	public class FilterRules {

		/*
		* Field order in the file
		*/
		public static final int TSMAC = 1;
		public static final int TSLOCATION = 2;
		public static final int COMMAND = 3;
		public static final int PROVIDER = 4;
		public static final int ERROR_DTI = 5;
		public static final int ERROR_PROVIDER = 6;
		public static final int TRIGGER_COUNT = 7;
		public static final int ALERT_ACTION = 8;

		public static final int FIELD_COUNT = 8;

		public static final String DFLT_FILTER_TEXT = "DTI alert filter";

		public int fileLine;
		public String command;
		public String tsmac;
		public String tslocation;
		public String provider;
		public String filterText;
		public String externalList;
		public int errorDTI;
		public int errorProvider;
		public int triggerCount;
		public int alertAction;
		public int ruleNumber;
		public int currentCount;

		FilterRules() {
			this.fileLine = 0;
			this.ruleNumber = 0;
			this.command = "*";
			this.tsmac = "*";
			this.tslocation = "*";
			this.provider = "*";
			this.errorDTI = 0;
			this.errorProvider = 0;
			this.triggerCount = 0;
			this.currentCount = 0;
			this.alertAction = AlertGlobals.SEND_BOTH;
			this.filterText = DFLT_FILTER_TEXT;
			this.externalList = "<n/a>";
		}
	}

}

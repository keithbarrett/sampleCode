package com.optum.devops.junitConnector;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.Date;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;


/**
 * @author kbarret9
 * Created on 4/13/2017
 * Some code originally by Vinay Kumar
 */

public class SonarJunitParser {
	
	private static int debugLevel = 1;
	

	/**
	 * @param urlString
	 * @return
	 */
	public static DataModel getDataObject(String urlString) {
		
		DataModel dataModel = new DataModel();
		boolean useFile = false; // read from file instead of web service
		Object resultObj = null;
		int n = 0;
		
		
		if (urlString == null) {
			if (debugLevel > 0) System.out.println("Debug: URL String is null - aborting");
			throw new RuntimeException("Failed : Passed URL string is null?");
		} else {
			if (debugLevel > 0)
				System.out.printf("Debug: Processing URL \"%s\"\n", urlString);
		}
			
		
		try {
			JSONParser parser = new JSONParser();			

			if (!useFile) {
				resultObj = parser.parse(getJSONData(urlString));
			} else {
				resultObj = parser.parse(new FileReader(urlString));
			}

			JSONObject parseObject = null;
			// TODO Handle responses with no values to process
			
			if (resultObj instanceof JSONObject) {
	        	if (debugLevel > 0) 
	        		System.out.println("Debug: Object " + resultObj);
	        	parseObject = (JSONObject) ((JSONObject) resultObj).get("component"); 
			} else if (resultObj instanceof JSONArray) {				
				parseObject = (JSONObject) ((JSONArray) resultObj).get(0);
			}  
	        
			
			/*
			 * Grab ID, Name and Date. These are always present
			 */
			
			Object id = parseObject.get("id");
			
			if (id == null)
				id = 0L;
			
			if (id instanceof java.lang.Long)
				dataModel.setId(Long.toString((java.lang.Long) id));
			else 
				dataModel.setId(id.toString());	
			
			dataModel.setName((String) parseObject.get("name"));
			//dataModel.setQualifier((String) parseObject.get("qualifier"));	
			
			SimpleDateFormat sdf = new SimpleDateFormat(dataModel.getSDF());	        
	        Date now = sdf.parse(sdf.format(new Date()));        

	        String jsonDate = (String) parseObject.get("date");

	        if (jsonDate != null) {
	        	String cleanDate = jsonDate.replaceAll("T", " "); // replace 'T'
	        	if (cleanDate.length() > 19)
	        		cleanDate = cleanDate.substring(0, 18);
	        	dataModel.setDate(sdf.parse(cleanDate));	
	        	// dataModel.setDate(sdf.parse(cleanDate));	
	        }
	        else
	        	dataModel.setDate(now);
	        	// If there's no date field in the json response, use today's date

	        if (debugLevel > 0)
	        	System.out.println("Debug: URL " + urlString + " - " + dataModel.getDate());
			
			org.json.simple.JSONArray measuresArray = 
					(org.json.simple.JSONArray) parseObject.get("measures");
			if (measuresArray == null)
				measuresArray = (org.json.simple.JSONArray) parseObject.get("msr");
			// Find the name of our metrics array
			
			if (measuresArray != null)
				n = measuresArray.size();
				// if null, then this URL returned no other metric data
			else
				if (debugLevel > 0)
					System.out.println("Debug: This URL has no metrics data");
			
			/*
			 * Process metrics array list
			 */
			
			for (int i = 0; i < n; ++i) {
				JSONObject measureJObj = (JSONObject) measuresArray.get(i);
				
				String metricName = null;
				float floatValue = (float) 0.0;
				@SuppressWarnings("unused")
				int intValue = 0;
				
				String jKeyName = "metric";
				String jValueName = "value";
				Object metricObj = measureJObj.get(jKeyName);
				
				if (metricObj == null) {
					jKeyName = "key";
					jValueName = "val";
					metricObj = measureJObj.get(jKeyName);
				}
				
				if (debugLevel > 0)
					System.out.printf("Debug: json parsing is using \"%s\" and \"%s\" pairs\n",
							jKeyName, jValueName);
				
				metricName = (String) measureJObj.get(jKeyName);
				
				if (measureJObj.get(jValueName) instanceof Double) {
					if (debugLevel > 0)
						System.out.println("Debug: Converting DOUBLE into Float and Int");
								// If it's a "double", let's turn it into a 3 decimal precision float
								
					Double double1 = (Double) measureJObj.get(jValueName);
					long tempLong = (long) (double1 * 1000L);
					floatValue = tempLong / 1000F;
					intValue = Math.round(floatValue);
					
				} else if (measureJObj.get(jValueName) instanceof Long) {
					Long long1 = (Long) measureJObj.get(jValueName);
					intValue = Integer.parseInt(long1 + "");
					// It's a long but it shouldn't ever exceed int values
					
				} else if (measureJObj.get(jValueName) instanceof String) {						
					if (measureJObj.get(jValueName).toString().contains(".")) {														
						//value = Integer.parseInt(measureJObj.get("value").toString().split("\\.")[0]);
						floatValue = Float.parseFloat(measureJObj.get(jValueName).toString());
					} else 
						intValue = Integer.parseInt((String) measureJObj.get(jValueName));
				
				} else if (measureJObj.get(jValueName) instanceof Float) {
					floatValue = (float) measureJObj.get(jValueName);
					intValue = Math.round(floatValue);
				}										
			
				
				switch (metricName) {
					case "tests" :
						dataModel.setJunitTests(floatValue);
						break;
						
					case "test_errors" :
						dataModel.setJunitErrors(floatValue);
						break;
						
					case "skipped_tests" :
						dataModel.setJunitSkipped(floatValue);
						break;
						
					case "test_failures" :
						dataModel.setJunitFailures(floatValue);
						break;
						
					default :
						if (debugLevel > 0)
							System.out.printf("Debug: Skipping unknown metrics field \"%s\"\n",
									metricName);
				};
			}			
		} catch (Exception e) {
			e.printStackTrace();
		}
		
		return dataModel;
	}
	
	
	
	/**
	 * Fetch json response
	 * 
	 * @param urlString URL to access
	 * @return json response
	 */
	private static String getJSONData(String urlString) {
		String str = "";
		
		try {				
 			URL url = new URL(urlString);
 			
			HttpURLConnection conn = (HttpURLConnection) url.openConnection();
			conn.setRequestMethod("GET");
			conn.setRequestProperty("Accept", "application/json");
			
			if (conn.getResponseCode() != 200) {	// Only "200" in the 200 series is OK
				throw new RuntimeException("Failed : HTTP error code : "
						+ conn.getResponseCode());
				// Should we throw, or should we just log and move on to the next?
			}
			
			BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
			String output = "";			
			
			while ((output = br.readLine()) != null) {
				str += output;
			}
	
			conn.disconnect();
		} catch (MalformedURLException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
		return str;
	}

}

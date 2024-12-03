#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

const char *sunrise_api_fmt = "https://api.sunrisesunset.io/json?lat=%s&lng=%s&time_format=24&timezone=UTC";

char sunrise_api_url[100] = "";
char *setSunriseURL(const char *geo_latitude, const char *geo_longitude)
{
    sprintf(sunrise_api_url, sunrise_api_fmt, geo_latitude, geo_longitude);
    return sunrise_api_url;
}

unsigned int parseSunriseHourAndMinuteFromHMS(const char *time)
{
    unsigned int timeHour, timeMin;
    if (sscanf(time, "%2u:%2u:00", &timeHour, &timeMin) != 2)
    {
        return 0;
    }
    return timeHour * 100 + timeMin;
}

int extractSunriseSunsetFromResponse(String responseBody, unsigned int *sunrise, unsigned int *sunset)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, responseBody);

    // Test if parsing succeeds.
    if (error)
    {
        Serial.printf("[SUNRISE] deserializeJson() failed: %s\n", error.c_str());
        return 3;
    }

    const char *sunriseText = doc["results"]["sunrise"];
    const char *sunsetText = doc["results"]["sunset"];

    int sunriseInt = parseSunriseHourAndMinuteFromHMS(sunriseText);
    int sunsetInt = parseSunriseHourAndMinuteFromHMS(sunsetText);
    if (sunriseInt == 0 || sunsetInt == 0)
    {
        Serial.printf("[SUNRISE] Failure parsing sunrise (%s) or sunset (%s) time...\n", sunriseText, sunsetText);
        return 4;
    }
    *sunrise = sunriseInt;
    *sunset = sunsetInt;
    return 0;
}

int QuerySunriseAndSunset(WiFiClientSecure wifiClient, const char *geo_latitude, const char *geo_longitude, unsigned int *sunriseOut, unsigned int *sunsetOut)
{
    HTTPClient https;
    setSunriseURL(geo_latitude, geo_longitude);

    // Initializing an HTTPS communication using the secure client
    Serial.printf("[SUNRISE] Querying sunrise API: %s\n", sunrise_api_url);
    if (https.begin(wifiClient, sunrise_api_url))
    { // HTTPS
        int httpCode = https.GET();
        if (httpCode > 0)
        {
            Serial.printf("[SUNRISE] HTTP response code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK)
            {
                String payload = https.getString();
                int ret = extractSunriseSunsetFromResponse(payload, sunriseOut, sunsetOut);
                return ret;
            }
        }
        else
        {
            Serial.printf("[SUNRISE] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
            return 2;
        }

        https.end();
    }
    else
    {
        Serial.printf("[SUNRISE] Unable to connect\n");
        return 1;
    }
    return 0;
}
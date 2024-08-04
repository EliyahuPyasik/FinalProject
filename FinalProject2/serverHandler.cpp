#include "serverHandler.h"
#include <sstream>
#include <iomanip>

namespace serverHandler {

    // Helper function to adjust the date backward by a certain number of days
    void adjustDate(int& year, int& month, int& day, int daysToSubtract) {
        static const int daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        day -= daysToSubtract;

        while (day <= 0) {
            month--;
            if (month <= 0) {
                month = 12;
                year--;
            }
            day += daysInMonth[(month - 1 + (year % 4 == 0 && month == 2)) % 12]; // Adjust February for leap years
        }
    }

    // Helper function to adjust the month backward by a certain number of months
    void adjustMonth(int& year, int& month, int day, int monthsToSubtract) {
        month -= monthsToSubtract;

        while (month <= 0) {
            month += 12;
            year--;
        }

        // Adjust day if necessary
        // (e.g., if the current day is not valid for the new month)
        static const int daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (day > daysInMonth[(month - 1 + (year % 4 == 0 && month == 2)) % 12]) {
            day = daysInMonth[(month - 1 + (year % 4 == 0 && month == 2)) % 12];
        }
    }

    // Helper function to adjust the year backward by a certain number of years
    void adjustYear(int& year, int& yearsToSubtract) {
        year -= yearsToSubtract;
    }

    std::string formatDate(int year, int month, int day) {
        std::ostringstream oss;
        oss << year << "-" << std::setw(2) << std::setfill('0') << month << "-" << std::setw(2) << std::setfill('0') << day;
        return oss.str();
    }

    httplib::Result getCurrencyInfo(int year, int month, int day, string currency) {
        // HTTPS
        string date = to_string(year) + '-' + (month < 10 ? '0' + to_string(month) : to_string(month)) + '-' + (day < 10 ? '0' + to_string(day) : to_string(day));
        string host = "cdn.jsdelivr.net";
        string path = "/npm/@fawazahmed0/currency-api@" + date + "/v1/currencies" + "/" + currency + ".json";

        httplib::SSLClient cli(host);
        httplib::Result res = cli.Get(path);

        if (res) {
            //std::cout << "Status: " << res->status << std::endl;
            //std::cout << "Body: " << res->body << std::endl;
        }
        else {
            std::cout << "Request failed" << std::endl;
        }
        return res;
    }

    // Helper function to extract a double value from a string
    double extractValue(const string& str) {
        try {
            return stod(str);
        }
        catch (...) {
            cerr << "Error converting to double: " << str << endl;
            return 0.0;
        }
    }

    // Function to get a value from a JSON string
    double getValueFromJson(const string& json, const string& key) {
        string keySearch = "\"" + key + "\":";
        size_t keyPos = json.find(keySearch);
        if (keyPos == string::npos) {
            cerr << "Key not found: " << key << endl;
            return 0.0;
        }

        size_t valueStart = json.find_first_of("0123456789.-", keyPos + keySearch.size());
        if (valueStart == string::npos) {
            cerr << "Value start not found for key: " << key << endl;
            return 0.0;
        }

        size_t valueEnd = json.find_first_of(",}", valueStart);
        if (valueEnd == string::npos) {
            valueEnd = json.find('}', valueStart);
        }

        string valueStr = json.substr(valueStart, valueEnd - valueStart);
        return extractValue(valueStr);
    }

    map<string, double> getInfo(int year, int month, int day, const string& mainCurrency, const string& secCurrency, duration d, int multiplication) {
        map<string, double> dateRateMap;

        // Helper function to add entry to the map
        auto addEntry = [&](int y, int m, int d) {
            string json = getCurrencyInfo(y, m, d, mainCurrency)->body;
            string formattedDate = formatDate(y, m, d);
            double secCurrencyValue = getValueFromJson(json, secCurrency);
            dateRateMap[formattedDate] = secCurrencyValue;
            };

        // Always include the current date
        addEntry(year, month, day);

        if (d == duration::weeks) {
            // Weekly: First day of each week
            for (int i = 1; i < multiplication; i++) {
                int tempYear = year;
                int tempMonth = month;
                int tempDay = day;

                // Adjust date to go back by i * 7 days
                adjustDate(tempYear, tempMonth, tempDay, i * 7);

                // Add entry to the map
                addEntry(tempYear, tempMonth, tempDay);
            }
        }
        else if (d == duration::months) {
            // Monthly: First day of each week within the month
            for (int i = 1; i <= multiplication; i++) {
                int tempYear = year;
                int tempMonth = month;

                // Adjust month to go back by i months
                adjustMonth(tempYear, tempMonth, 1, i);

                // Get the first day of the month
                addEntry(tempYear, tempMonth, 1);

                // Iterate over each week in the month
                for (int weekStart = 1; weekStart <= 31; weekStart += 7) {
                    int tempDay = weekStart;

                    // Ensure the day is within the valid range for the current month
                    if (tempDay <= 31) { // Simplified range check
                        addEntry(tempYear, tempMonth, tempDay);
                    }
                }
            }
        }
        else if (d == duration::years) {
            // Yearly: First day of each month within the year
            for (int i = 0; i < multiplication; i++) {
                int tempYear = year;

                // Adjust year to go back by i years
                adjustYear(tempYear, i);

                for (int m = 1; m <= 12; m++) {
                    addEntry(tempYear, m, 1);
                }
            }
        }
        else if (d == duration::days) {
            // Daily: Go back a certain number of days
            for (int i = 1; i <= multiplication; i++) {
                int tempYear = year;
                int tempMonth = month;
                int tempDay = day;

                // Adjust date to go back by i days
                adjustDate(tempYear, tempMonth, tempDay, i);

                // Add entry to the map
                addEntry(tempYear, tempMonth, tempDay);
            }
        }

        return dateRateMap;
    }

} // namespace serverHandler

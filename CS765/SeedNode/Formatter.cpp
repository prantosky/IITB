#include <iostream>
#include <regex>
#include <vector>

using namespace std;

class Formatter {
   private:
	/* data */
   public:
	Formatter(/* args */) = delete;
	~Formatter() = delete;
	static bool validateIP(const string& ip);
	static array<string, 5> parse(const string& ip);
};

bool Formatter::validateIP(const string& ip) {
	string octal = R"(([0-1]?\d{1,2}|2[0-4]\d|25[0-5]\.))";
	regex re(octal + R"(\.)" + octal + R"(\.)" + octal + R"(\.)" + octal);
	if (regex_match(ip.c_str(), re)) {
		return true;
	}
	cout << "Formatter::validateIP: IP validation failed for string " << ip
		 << endl;
	return false;
}

array<string, 5> Formatter::parse(const string& str) {
	array<string, 5> arr;
	const regex re(R"((.+?):(.+?):(\d+?):(.+?):(.+))");

	smatch match;
	if (not regex_search(str, match, re)) {
		cerr << "Formatter::parse: Parsing failed for string \"" << str << "\""
			 << endl;
	}
	for (size_t i = 1; i <= 5; i++) {
		arr[i - 1] = match.str(i);
	}
	return arr;
}

// int main() {
// 	string s =
// 		"Dead Node:<DeadNode.IP>:<DeadNode.ports>:<self.timestamp>:<self.IP>";
// 	auto arr = Formatter::parse(s);
// 	return 0;
// }
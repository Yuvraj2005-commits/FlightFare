#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define NOMINMAX

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include "httplib.h"
#include "json.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <algorithm>
#include <limits>
#include <cctype>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const int INF = 1e9;  // Safe infinity value (avoids overflow)
const int MAX_STOPS_LIMIT = 50;  // Reasonable limit for flights

// =======================================
// Utility Functions
// =======================================
string toLowerCase(const string &s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(), 
              [](unsigned char c){ return tolower(c); });
    return result;
}

string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == string::npos) return "";
    return s.substr(start, end - start + 1);
}

// =======================================
// Levenshtein Distance (for suggestions)
// =======================================
int levenshtein(const string &a, const string &b) {
    int n = a.size(), m = b.size();
    if (n == 0) return m;
    if (m == 0) return n;
    
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));

    for (int i = 0; i <= n; i++) dp[i][0] = i;
    for (int j = 0; j <= m; j++) dp[0][j] = j;

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            if (a[i-1] == b[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }
    return dp[n][m];
}

vector<string> suggestCities(const string &wrong, const vector<string> &cities) {
    vector<pair<int, string>> scores;
    string wrongLower = toLowerCase(wrong);
    
    for (const auto &c : cities) {
        int dist = levenshtein(wrongLower, toLowerCase(c));
        scores.push_back({dist, c});
    }
    
    sort(scores.begin(), scores.end());
    
    vector<string> res;
    int limit = min(3, (int)scores.size());
    for (int i = 0; i < limit; i++) {
        // Only suggest if distance is reasonable (not too different)
        if (scores[i].first <= 3 || scores[i].first <= (int)wrongLower.length() / 2) {
            res.push_back(scores[i].second);
        }
    }
    return res;
}

// =======================================
// Graph + Optimized Dijkstra with Stops
// =======================================
class Graph {
public:
    int V;
    vector<vector<pair<int, int>>> adj;  // (neighbor, cost)
    
    Graph(int V = 0) {
        this->V = V;
        adj.assign(V, {});
    }

    void addEdge(int u, int v, int cost) {
        if (u < 0 || v < 0 || u >= V || v >= V) {
            cerr << "Error: Invalid edge indices: " << u << " -> " << v << endl;
            return;
        }
        if (cost < 0) {
            cerr << "Error: Negative cost not allowed" << endl;
            return;
        }
        adj[u].push_back({v, cost});
    }

    // Bellman-Ford variant optimized for K stops constraint
    pair<int, vector<int>> shortestPathWithStops(int src, int dest, int maxStops) {
        if (src < 0 || dest < 0 || src >= V || dest >= V) {
            return {-1, {}};
        }
        
        if (src == dest) {
            return {0, {src}};
        }
        
        // dist[i] = minimum cost to reach node i
        vector<int> dist(V, INF);
        vector<int> parent(V, -1);
        
        dist[src] = 0;
        
        // Relax edges at most (maxStops + 1) times
        for (int iteration = 0; iteration <= maxStops; iteration++) {
            vector<int> newDist = dist;
            vector<int> newParent = parent;
            
            bool updated = false;
            
            for (int u = 0; u < V; u++) {
                if (dist[u] >= INF) continue;
                
                for (const auto &[v, cost] : adj[u]) {
                    if (dist[u] + cost < newDist[v]) {
                        newDist[v] = dist[u] + cost;
                        newParent[v] = u;
                        updated = true;
                    }
                }
            }
            
            dist = newDist;
            parent = newParent;
            
            // Early termination if no updates
            if (!updated) break;
        }
        
        // Check if destination is reachable
        if (dist[dest] >= INF) {
            return {-1, {}};
        }
        
        // Reconstruct path
        vector<int> path;
        int cur = dest;
        while (cur != -1) {
            path.push_back(cur);
            cur = parent[cur];
        }
        reverse(path.begin(), path.end());
        
        // Verify path validity
        if (path.empty() || path[0] != src) {
            return {-1, {}};
        }
        
        // Check if stops constraint is satisfied
        int stops = path.size() - 2;  // stops = intermediate cities
        if (stops > maxStops) {
            return {-1, {}};
        }
        
        return {dist[dest], path};
    }
};

// =======================================
// Global Data (Cities + Flights)
// =======================================
vector<string> idToCity;
unordered_map<string, int> cityToID;
unordered_map<string, int> cityToIDLower;  // For case-insensitive lookup
Graph g;

void initData() {
    // Expanded city list with more connections
    idToCity = {
        "Delhi", "Mumbai", "Goa", "Chennai", "Jaipur",
        "Bangalore", "Kolkata", "Hyderabad", "Pune", "Ahmedabad"
    };

    cityToID.clear();
    cityToIDLower.clear();
    
    for (int i = 0; i < (int)idToCity.size(); ++i) {
        cityToID[idToCity[i]] = i;
        cityToIDLower[toLowerCase(idToCity[i])] = i;
    }

    g = Graph(idToCity.size());

    // Sample directed flights: source -> destination (cost)
    // Delhi connections
    g.addEdge(cityToID["Delhi"], cityToID["Mumbai"], 5000);
    g.addEdge(cityToID["Delhi"], cityToID["Goa"], 7000);
    g.addEdge(cityToID["Delhi"], cityToID["Bangalore"], 6500);
    g.addEdge(cityToID["Delhi"], cityToID["Kolkata"], 5500);
    
    // Mumbai connections
    g.addEdge(cityToID["Mumbai"], cityToID["Chennai"], 5500);
    g.addEdge(cityToID["Mumbai"], cityToID["Goa"], 3000);
    g.addEdge(cityToID["Mumbai"], cityToID["Bangalore"], 4500);
    g.addEdge(cityToID["Mumbai"], cityToID["Pune"], 2000);
    
    // Goa connections
    g.addEdge(cityToID["Goa"], cityToID["Chennai"], 4500);
    g.addEdge(cityToID["Goa"], cityToID["Bangalore"], 3500);
    
    // Chennai connections
    g.addEdge(cityToID["Chennai"], cityToID["Jaipur"], 8000);
    g.addEdge(cityToID["Chennai"], cityToID["Hyderabad"], 3000);
    g.addEdge(cityToID["Chennai"], cityToID["Bangalore"], 2500);
    
    // Bangalore connections
    g.addEdge(cityToID["Bangalore"], cityToID["Hyderabad"], 3000);
    g.addEdge(cityToID["Bangalore"], cityToID["Pune"], 4000);
    
    // Kolkata connections
    g.addEdge(cityToID["Kolkata"], cityToID["Chennai"], 6000);
    g.addEdge(cityToID["Kolkata"], cityToID["Hyderabad"], 5500);
    
    // Jaipur connections
    g.addEdge(cityToID["Jaipur"], cityToID["Mumbai"], 4500);
    g.addEdge(cityToID["Jaipur"], cityToID["Ahmedabad"], 3500);
    
    // Additional connections
    g.addEdge(cityToID["Hyderabad"], cityToID["Pune"], 3500);
    g.addEdge(cityToID["Pune"], cityToID["Ahmedabad"], 4000);
    g.addEdge(cityToID["Ahmedabad"], cityToID["Delhi"], 4500);
}

string getCityNameCaseInsensitive(const string &input) {
    string inputLower = toLowerCase(trim(input));
    
    if (cityToIDLower.find(inputLower) != cityToIDLower.end()) {
        int id = cityToIDLower[inputLower];
        return idToCity[id];
    }
    
    return "";
}

// =======================================
// Main: HTTP API server
// =======================================
int main() {
    initData();

    httplib::Server svr;

    // Handle CORS preflight
    svr.Options(R"(.*)", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    // GET /api/cities - List all available cities
    svr.Get("/api/cities", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        json response;
        response["cities"] = idToCity;
        response["count"] = idToCity.size();
        
        res.status = 200;
        res.set_content(response.dump(2), "application/json");
    });

    // GET /health - Health check endpoint
    svr.Get("/health", [](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        json response;
        response["status"] = "healthy";
        response["service"] = "Flight Fare Finder";
        response["cities_loaded"] = idToCity.size();
        
        res.status = 200;
        res.set_content(response.dump(2), "application/json");
    });

    // POST /api/search - Search for cheapest flight
    svr.Post("/api/search", [](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");

        json response;

        try {
            auto bodyJson = json::parse(req.body);

            // Extract and validate input
            if (!bodyJson.contains("src") || !bodyJson.contains("dest") || 
                !bodyJson.contains("maxStops")) {
                response["error"] = "Missing required fields: src, dest, maxStops";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            string srcInput = trim(bodyJson.at("src").get<string>());
            string destInput = trim(bodyJson.at("dest").get<string>());
            int maxStops = bodyJson.at("maxStops").get<int>();

            // Case-insensitive city lookup
            string srcCity = getCityNameCaseInsensitive(srcInput);
            string destCity = getCityNameCaseInsensitive(destInput);

            // Validate Source
            if (srcCity.empty()) {
                auto suggestions = suggestCities(srcInput, idToCity);
                response["error"] = "Invalid source city: " + srcInput;
                if (!suggestions.empty()) {
                    response["suggestions"] = suggestions;
                }
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Validate Destination
            if (destCity.empty()) {
                auto suggestions = suggestCities(destInput, idToCity);
                response["error"] = "Invalid destination city: " + destInput;
                if (!suggestions.empty()) {
                    response["suggestions"] = suggestions;
                }
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Check if source and destination are the same
            if (srcCity == destCity) {
                response["error"] = "Source and destination must be different";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            // Validate maxStops
            if (maxStops < 0) {
                response["error"] = "Max stops must be non-negative";
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            if (maxStops > MAX_STOPS_LIMIT) {
                response["error"] = "Max stops cannot exceed " + to_string(MAX_STOPS_LIMIT);
                res.status = 400;
                res.set_content(response.dump(2), "application/json");
                return;
            }

            int src = cityToID[srcCity];
            int dest = cityToID[destCity];

            auto result = g.shortestPathWithStops(src, dest, maxStops);

            if (result.first == -1 || result.second.empty()) {
                response["error"] = "No route found within " + to_string(maxStops) + " stops";
                response["from"] = srcCity;
                response["to"] = destCity;
                response["maxStops"] = maxStops;
                res.status = 404;
            } else {
                response["fare"] = result.first;
                
                vector<string> route;
                for (int id : result.second) {
                    route.push_back(idToCity[id]);
                }
                response["route"] = route;
                response["stops"] = (int)route.size() - 2;  // Intermediate cities
                response["flights"] = (int)route.size() - 1;  // Number of flights
                
                res.status = 200;
            }
        } catch (const json::exception &e) {
            response["error"] = "Invalid JSON format: " + string(e.what());
            res.status = 400;
        } catch (const exception &e) {
            response["error"] = "Server error: " + string(e.what());
            res.status = 500;
        }

        res.set_content(response.dump(2), "application/json");
    });

    // 404 handler
    svr.set_error_handler([](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json response;
        response["error"] = "Endpoint not found";
        response["available_endpoints"] = {
            "GET /api/cities",
            "POST /api/search",
            "GET /health"
        };
        res.set_content(response.dump(2), "application/json");
    });

    cout << "🚀 C++ Flight Fare Server running on http://localhost:8080\n";
    cout << "📍 Available endpoints:\n";
    cout << "   GET  /api/cities  - List all cities\n";
    cout << "   POST /api/search  - Search flights\n";
    cout << "   GET  /health      - Health check\n";
    cout << "\n";
    cout << "Example request:\n";
    cout << R"(  curl -X POST http://localhost:8080/api/search \)" << "\n";
    cout << R"(       -H "Content-Type: application/json" \)" << "\n";
    cout << R"(       -d '{"src":"Delhi","dest":"Chennai","maxStops":2}')" << "\n";
    
    svr.listen("0.0.0.0", 8080);
    return 0;
}
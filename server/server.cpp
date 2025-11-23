#define _WIN32_WINNT 0x0A00
#define NOMINMAX

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <algorithm>
#include <limits>
#include <set>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const int INF = 1e9;
const int MAX_STOPS_LIMIT = 50;
const int DEFAULT_MAX_STOPS = 5;

// ------------------------ Utility Functions ------------------------

string toLowerCase(const string &s) {
    string out = s;
    transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    size_t b = s.find_last_not_of(" \t\n\r");
    if (a == string::npos) return "";
    return s.substr(a, b - a + 1);
}

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void logRequest(const string& method, const string& path, int status) {
    cout << "[" << getCurrentTimestamp() << "] " 
         << method << " " << path << " - Status: " << status << endl;
}

// ------------------------ Levenshtein Distance ------------------------

int levenshtein(const string &a, const string &b) {
    int n = a.size(), m = b.size();
    if (n == 0) return m;
    if (m == 0) return n;
    
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (int i = 0; i <= n; i++) dp[i][0] = i;
    for (int j = 0; j <= m; j++) dp[0][j] = j;

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            dp[i][j] = (a[i - 1] == b[j - 1])
                           ? dp[i - 1][j - 1]
                           : 1 + min({ dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1] });
        }
    }

    return dp[n][m];
}

vector<string> suggestCities(const string &wrong, const vector<string> &cities) {
    vector<pair<int, string>> scores;
    string wrongLower = toLowerCase(wrong);
    
    for (const auto &c : cities) {
        int dist = levenshtein(wrongLower, toLowerCase(c));
        // Only suggest if distance is reasonable
        if (dist <= 3) {
            scores.push_back({ dist, c });
        }
    }
    
    sort(scores.begin(), scores.end());

    vector<string> result;
    for (int i = 0; i < min(3, (int)scores.size()); i++) {
        result.push_back(scores[i].second);
    }
    return result;
}

// ------------------------ Graph Logic ------------------------

struct Route {
    int cost;
    vector<int> path;
    int stops;
};

class Graph {
public:
    int V;
    vector<vector<pair<int, int>>> adj;

    Graph(int v = 0) { 
        V = v; 
        adj.assign(V, {}); 
    }

    void addEdge(int u, int v, int cost) {
        if (cost >= 0 && u >= 0 && u < V && v >= 0 && v < V) {
            adj[u].push_back({ v, cost });
        }
    }

    void addBidirectionalEdge(int u, int v, int cost) {
        addEdge(u, v, cost);
        addEdge(v, u, cost);
    }

    // Find cheapest path with maximum stops constraint
    pair<int, vector<int>> shortest(int src, int dest, int maxStops) {
        if (src == dest) {
            return { 0, {src} };
        }

        vector<int> dist(V, INF), parent(V, -1);
        dist[src] = 0;

        // Bellman-Ford with stop limit
        for (int k = 0; k <= maxStops; k++) {
            bool updated = false;
            vector<int> newDist = dist;
            vector<int> newParent = parent;

            for (int u = 0; u < V; u++) {
                if (dist[u] >= INF) continue;
                
                for (auto &edge : adj[u]) {
                    int v = edge.first;
                    int weight = edge.second;
                    
                    if (dist[u] + weight < newDist[v]) {
                        newDist[v] = dist[u] + weight;
                        newParent[v] = u;
                        updated = true;
                    }
                }
            }
            
            dist = newDist;
            parent = newParent;
            
            if (!updated) break;
        }

        if (dist[dest] >= INF) {
            return { -1, {} };
        }

        // Reconstruct path
        vector<int> path;
        for (int v = dest; v != -1; v = parent[v]) {
            path.push_back(v);
        }
        reverse(path.begin(), path.end());
        
        return { dist[dest], path };
    }

    // Find multiple route options (using modified Dijkstra)
    vector<Route> findMultipleRoutes(int src, int dest, int maxStops, int maxResults = 3) {
        if (src == dest) {
            return { {0, {src}, 0} };
        }

        // Priority queue: (cost, node, path, stops)
        priority_queue<tuple<int, int, vector<int>, int>,
                      vector<tuple<int, int, vector<int>, int>>,
                      greater<tuple<int, int, vector<int>, int>>> pq;
        
        pq.push({0, src, {src}, 0});
        
        vector<Route> results;
        set<int> visitedCosts; // Track different cost routes found
        
        while (!pq.empty() && results.size() < maxResults) {
            auto [cost, node, path, stops] = pq.top();
            pq.pop();
            
            if (node == dest) {
                // Avoid duplicate costs
                if (visitedCosts.find(cost) == visitedCosts.end()) {
                    results.push_back({cost, path, stops});
                    visitedCosts.insert(cost);
                }
                continue;
            }
            
            if (stops >= maxStops) continue;
            
            for (auto &edge : adj[node]) {
                int nextNode = edge.first;
                int edgeCost = edge.second;
                
                // Avoid cycles
                if (find(path.begin(), path.end(), nextNode) != path.end()) {
                    continue;
                }
                
                vector<int> newPath = path;
                newPath.push_back(nextNode);
                
                pq.push({cost + edgeCost, nextNode, newPath, stops + 1});
            }
        }
        
        return results;
    }
};

// ------------------------ Data Management ------------------------

vector<string> idToCity;
unordered_map<string, int> cityMap;
Graph g;

void initData() {
    idToCity = {
        "Delhi", "Mumbai", "Goa", "Chennai", "Jaipur",
        "Bangalore", "Kolkata", "Hyderabad", "Pune", "Ahmedabad",
        "Kochi", "Varanasi", "Amritsar", "Lucknow", "Chandigarh", "Patna"
    };

    cityMap.clear();
    for (size_t i = 0; i < idToCity.size(); i++) {
        cityMap[toLowerCase(idToCity[i])] = i;
    }

    g = Graph(idToCity.size());

    // Define bidirectional routes (more realistic)
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["mumbai"], 5000);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["goa"], 7000);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["bangalore"], 6500);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["kolkata"], 5500);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["jaipur"], 3000);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["chandigarh"], 2500);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["lucknow"], 3500);
    g.addBidirectionalEdge(cityMap["delhi"], cityMap["amritsar"], 4000);

    g.addBidirectionalEdge(cityMap["mumbai"], cityMap["chennai"], 5500);
    g.addBidirectionalEdge(cityMap["mumbai"], cityMap["goa"], 3000);
    g.addBidirectionalEdge(cityMap["mumbai"], cityMap["pune"], 1500);
    g.addBidirectionalEdge(cityMap["mumbai"], cityMap["ahmedabad"], 4000);

    g.addBidirectionalEdge(cityMap["goa"], cityMap["chennai"], 4500);
    g.addBidirectionalEdge(cityMap["goa"], cityMap["bangalore"], 3500);
    g.addBidirectionalEdge(cityMap["goa"], cityMap["kochi"], 4000);

    g.addBidirectionalEdge(cityMap["chennai"], cityMap["hyderabad"], 2500);
    g.addBidirectionalEdge(cityMap["chennai"], cityMap["bangalore"], 2000);
    g.addBidirectionalEdge(cityMap["chennai"], cityMap["kochi"], 3500);

    g.addBidirectionalEdge(cityMap["bangalore"], cityMap["hyderabad"], 3000);
    g.addBidirectionalEdge(cityMap["bangalore"], cityMap["pune"], 4000);
    g.addBidirectionalEdge(cityMap["bangalore"], cityMap["kochi"], 3000);

    g.addBidirectionalEdge(cityMap["kolkata"], cityMap["chennai"], 6000);
    g.addBidirectionalEdge(cityMap["kolkata"], cityMap["hyderabad"], 5500);
    g.addBidirectionalEdge(cityMap["kolkata"], cityMap["varanasi"], 3500);
    g.addBidirectionalEdge(cityMap["kolkata"], cityMap["lucknow"], 4500);
    g.addBidirectionalEdge(cityMap["kolkata"], cityMap["patna"], 2500);

    g.addBidirectionalEdge(cityMap["jaipur"], cityMap["ahmedabad"], 3500);
    g.addBidirectionalEdge(cityMap["jaipur"], cityMap["mumbai"], 4500);

    g.addBidirectionalEdge(cityMap["hyderabad"], cityMap["pune"], 2200);
    g.addBidirectionalEdge(cityMap["pune"], cityMap["ahmedabad"], 3500);
    g.addBidirectionalEdge(cityMap["ahmedabad"], cityMap["delhi"], 4500);

    g.addBidirectionalEdge(cityMap["varanasi"], cityMap["lucknow"], 2000);
    g.addBidirectionalEdge(cityMap["varanasi"], cityMap["patna"], 1500);
    g.addBidirectionalEdge(cityMap["lucknow"], cityMap["chandigarh"], 4000);
    g.addBidirectionalEdge(cityMap["lucknow"], cityMap["patna"], 2800);
    g.addBidirectionalEdge(cityMap["chandigarh"], cityMap["amritsar"], 2500);
    g.addBidirectionalEdge(cityMap["patna"], cityMap["delhi"], 4800);

    cout << "✓ Loaded " << idToCity.size() << " cities and routes" << endl;
}

// ------------------------ CORS Middleware ------------------------

void enableCORS(httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// ------------------------ API Handlers ------------------------

void handleGetCities(const httplib::Request &req, httplib::Response &res) {
    enableCORS(res);
    
    json response;
    response["cities"] = idToCity;
    response["totalCities"] = idToCity.size();
    
    res.set_content(response.dump(2), "application/json");
    res.status = 200;
    
    logRequest("GET", "/cities", 200);
}

void handleSearch(const httplib::Request &req, httplib::Response &res) {
    enableCORS(res);
    json response;

    try {
        auto body = json::parse(req.body);
        
        // Validate required fields
        if (!body.contains("src") || !body.contains("dest")) {
            response["error"] = "Missing required fields: 'src' and 'dest'";
            res.set_content(response.dump(2), "application/json");
            res.status = 400;
            logRequest("POST", "/search", 400);
            return;
        }
        
        string src = toLowerCase(trim(body["src"]));
        string dest = toLowerCase(trim(body["dest"]));
        
        // Parse maxStops with default value
        int maxStops = DEFAULT_MAX_STOPS;
        if (body.contains("maxStops")) {
            if (body["maxStops"].is_string()) {
                try {
                    maxStops = stoi(body["maxStops"].get<string>());
                } catch (...) {
                    response["error"] = "Invalid maxStops value";
                    res.set_content(response.dump(2), "application/json");
                    res.status = 400;
                    logRequest("POST", "/search", 400);
                    return;
                }
            } else if (body["maxStops"].is_number()) {
                maxStops = body["maxStops"];
            }
        }
        
        // Validate maxStops range
        if (maxStops < 0 || maxStops > MAX_STOPS_LIMIT) {
            response["error"] = "maxStops must be between 0 and " + to_string(MAX_STOPS_LIMIT);
            res.set_content(response.dump(2), "application/json");
            res.status = 400;
            logRequest("POST", "/search", 400);
            return;
        }
        
        // Check if same city
        if (src == dest) {
            response["error"] = "Source and destination cannot be the same";
            res.set_content(response.dump(2), "application/json");
            res.status = 400;
            logRequest("POST", "/search", 400);
            return;
        }
        
        // Validate city names
        if (!cityMap.count(src)) {
            response["error"] = "Invalid source city: " + body["src"].get<string>();
            auto suggestions = suggestCities(src, idToCity);
            if (!suggestions.empty()) {
                response["suggestions"] = suggestions;
            }
            res.set_content(response.dump(2), "application/json");
            res.status = 400;
            logRequest("POST", "/search", 400);
            return;
        }
        
        if (!cityMap.count(dest)) {
            response["error"] = "Invalid destination city: " + body["dest"].get<string>();
            auto suggestions = suggestCities(dest, idToCity);
            if (!suggestions.empty()) {
                response["suggestions"] = suggestions;
            }
            res.set_content(response.dump(2), "application/json");
            res.status = 400;
            logRequest("POST", "/search", 400);
            return;
        }
        
        // Find routes
        bool findMultiple = body.contains("multiple") && body["multiple"] == true;
        
        if (findMultiple) {
            auto routes = g.findMultipleRoutes(cityMap[src], cityMap[dest], maxStops, 3);
            
            if (routes.empty()) {
                response["error"] = "No routes found within " + to_string(maxStops) + " stops";
                response["suggestion"] = "Try increasing maxStops limit";
                res.status = 404;
            } else {
                json routeArray = json::array();
                for (const auto& route : routes) {
                    json routeObj;
                    routeObj["fare"] = route.cost;
                    routeObj["stops"] = route.stops;
                    
                    vector<string> cityNames;
                    for (int id : route.path) {
                        cityNames.push_back(idToCity[id]);
                    }
                    routeObj["route"] = cityNames;
                    routeArray.push_back(routeObj);
                }
                response["routes"] = routeArray;
                response["totalRoutes"] = routes.size();
                res.status = 200;
            }
        } else {
            auto result = g.shortest(cityMap[src], cityMap[dest], maxStops);
            
            if (result.first == -1) {
                response["error"] = "No route found within " + to_string(maxStops) + " stops";
                response["suggestion"] = "Try increasing maxStops limit";
                res.status = 404;
            } else {
                response["fare"] = result.first;
                response["stops"] = (int)result.second.size() - 1;
                
                vector<string> cityNames;
                for (int id : result.second) {
                    cityNames.push_back(idToCity[id]);
                }
                response["route"] = cityNames;
                res.status = 200;
            }
        }
        
    } catch (const json::parse_error& e) {
        response["error"] = "Invalid JSON format";
        response["details"] = e.what();
        res.status = 400;
    } catch (const json::type_error& e) {
        response["error"] = "Invalid field type in JSON";
        response["details"] = e.what();
        res.status = 400;
    } catch (const exception& e) {
        response["error"] = "Internal server error";
        response["details"] = e.what();
        res.status = 500;
    }

    res.set_content(response.dump(2), "application/json");
    logRequest("POST", "/search", res.status);
}

void handleHealth(const httplib::Request &req, httplib::Response &res) {
    enableCORS(res);
    
    json response;
    response["status"] = "healthy";
    response["timestamp"] = getCurrentTimestamp();
    response["version"] = "2.0";
    
    res.set_content(response.dump(2), "application/json");
    res.status = 200;
    
    logRequest("GET", "/health", 200);
}

// ------------------------ Web Server ------------------------

int main() {
    cout << "\n================================" << endl;
    cout << "  Flight Route Finder API v2.0  " << endl;
    cout << "================================\n" << endl;
    
    initData();
    
    httplib::Server svr;

    // OPTIONS handler for CORS preflight
    svr.Options(".*", [](const httplib::Request &req, httplib::Response &res) { 
        enableCORS(res); 
        res.status = 204; 
    });

    // API Routes
    svr.Get("/cities", handleGetCities);
    svr.Post("/search", handleSearch);
    svr.Get("/health", handleHealth);
    
    // Root endpoint
    svr.Get("/", [](const httplib::Request &req, httplib::Response &res) {
        enableCORS(res);
        json response;
        response["message"] = "Flight Route Finder API";
        response["version"] = "2.0";
        response["endpoints"] = {
            {"GET /cities", "List all available cities"},
            {"POST /search", "Find cheapest route (body: {src, dest, maxStops?, multiple?})"},
            {"GET /health", "Check API health status"}
        };
        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        logRequest("GET", "/", 200);
    });

    cout << "\n🚀 Server running at: http://localhost:8080" << endl;
    cout << "📍 Endpoints:" << endl;
    cout << "   GET  /cities  - List all cities" << endl;
    cout << "   POST /search  - Find routes" << endl;
    cout << "   GET  /health  - Health check" << endl;
    cout << "\n⏳ Waiting for requests...\n" << endl;
    
    svr.listen("0.0.0.0", 8080);
    
    return 0;
}
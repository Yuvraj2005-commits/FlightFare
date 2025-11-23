# ✈️ Flight Fare Finder – C++ + Web UI

A mini project that finds the **cheapest flight route** between cities using a **C++ backend server** and a **beautiful HTML/JS frontend**.  
It uses **graph algorithms** with a maximum–stops constraint, plus **auto-suggestions** for misspelled city names.

---

## 📌 Features

- 🔍 Find cheapest route between two cities  
- ⏱ Limit maximum number of stops  
- 🧠 City name auto-correction using Levenshtein distance  
- 🌐 C++ HTTP server using `cpp-httplib`  
- 📄 JSON API using `nlohmann::json`  
- 💻 Responsive frontend with clean UI  
- 🚫 Works fully offline on `localhost`

---

## 🛠 Tech Stack

| Layer      | Technology                  |
|-----------|-----------------------------|
| Backend   | C++17                       |
| HTTP Lib  | `cpp-httplib`               |
| JSON Lib  | `nlohmann/json` (`json.hpp`)|
| Frontend  | HTML5, CSS3, Vanilla JS     |
| OS        | Windows (MinGW-w64 g++)     |

---

## 📁 Project Structure

```text
FlightFareProject/
│
├── server/
│   ├── server.cpp      # C++ backend code
│   ├── httplib.h       # HTTP server single-header library
│   ├── json.hpp        # nlohmann json single-header library
│   └── server.exe      # compiled backend (generated)
│
└── client/
    └── index.html      # Frontend UI

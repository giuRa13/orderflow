#include <network_layer.h>
#include <GLFW/glfw3.h>
#include <chrono>

NetworkLayer::NetworkLayer(MarketData& data) : m_data(data) 
{
    ix::initNetSystem(); 
    m_ws.setPingInterval(30);
    //m_ws.enableAutomaticReconnection();
}

NetworkLayer::~NetworkLayer()
{
    this->end(); 
    ix::uninitNetSystem();
}

void NetworkLayer::end()
{
    m_ws.stop();
} 

double GetTimeNow() 
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

void NetworkLayer::start_multi(const std::set<std::string>& symbols, bool is_futures) 
{        
    std::string base = is_futures ? 
        "wss://fstream.binance.com/stream?streams=" 
        : "wss://stream.binance.com:9443/stream?streams=";

    std::string stream_path = "";
    for (auto& s : symbols) 
    {
        std::string sym = s;
        std::transform(sym.begin(), sym.end(), sym.begin(), ::tolower);
        stream_path += sym + "@aggTrade/" + sym + "@bookTicker/" + sym + "@depth@100ms/";
    }

    if (!stream_path.empty()) stream_path.pop_back(); // remove last '/'

    m_ws.setUrl(base + stream_path);

    m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) 
        {
            try {
                auto root = nlohmann::json::parse(msg->str);

                std::string stream = root["stream"]; // DO NOT tolower this
                auto& data = root["data"];
                // Get the event type (Always "aggTrade", "depthUpdate", or "bookTicker")
            if (!data.contains("e")) return;
            std::string event_type = data["e"].get<std::string>();
                std::string symbol = stream.substr(0, stream.find('@'));
                std::string stream_lower = stream;
                std::transform(stream_lower.begin(), stream_lower.end(), stream_lower.begin(), ::tolower);

                std::lock_guard<std::recursive_mutex> lock(m_data.mtx);
                auto& sData = m_data.get(symbol);

                /*if (stream_lower.find("@depth") != std::string::npos) 
                {
                    sData.last_depth_time = GetTimeNow();
                    process_depth_diff(symbol, data);
                } 
                else if (stream_lower.find("@aggtrade") != std::string::npos) // Lowercase T
                {
                    sData.last_trade_time = GetTimeNow(); 
                    process_tick_data(symbol, data);
                }
                else if (stream_lower.find("@bookticker") != std::string::npos) // Lowercase t
                {
                    process_book_ticker(symbol, data);
                }*/
                // IDENTIFY STREAM BY EVENT TYPE (NOT BY STRING FIND)
            if (event_type == "depthUpdate") 
            {
                sData.last_depth_time = GetTimeNow();
                process_depth_diff(symbol, data);
            } 
            else if (event_type == "aggTrade") 
            {
                sData.last_trade_time = GetTimeNow(); 
                process_tick_data(symbol, data);
            }
            else if (event_type == "bookTicker") 
            {
                process_book_ticker(symbol, data);
            }
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "[JSON] Global Parse Error: " << e.what() << std::endl;
            }
        }
        else if (msg->type == ix::WebSocketMessageType::Open) 
        {
            connection_status = 1; // Stay Orange/Yellow until first data packet
            std::cout << "[WS] Successfully connected to Binance Futures!" << std::endl;
        } 
        else if (msg->type == ix::WebSocketMessageType::Error)
        {
            connection_status = 0;
            std::cerr << "[WS] Error: " << msg->errorInfo.reason << std::endl;
        }
        else if (msg->type == ix::WebSocketMessageType::Close) 
        {
            connection_status = 0;
            std::cout << "[WS] Connection Closed." << std::endl;
        }
    });
    m_ws.start();
}

void NetworkLayer::process_tick_data(const std::string& symbol, const nlohmann::json& j) 
{
    //std::cout << "!!!!!!!!!  Tick received for " << symbol << " Price: " << j["p"] << std::endl;
    try 
    {
       // Robust parsing for p and q (handle both string and number)
        double price = j["p"].is_string() ? std::stod(j["p"].get<std::string>()) : j["p"].get<double>();
        double qty   = j["q"].is_string() ? std::stod(j["q"].get<std::string>()) : j["q"].get<double>();

        // Trade time handling
        long long raw_time = j["T"].get<long long>();
        double time = (double)raw_time / 1000.0;
        
        // Side handling (m = buyer is maker)
        bool is_sell = j["m"].get<bool>();
        double delta = is_sell ? -qty : qty;

        SymbolData& sData = m_data.get(symbol); // Mutex already locked in caller
        
        // Update DOM Market Orders
        double step = m_data.dom_step;
        double bucket_p = std::floor(price / step) * step;
        if (is_sell) {
            sData.market_sells[bucket_p] += qty;
            sData.last_sell_time[bucket_p] = time;
        } else {
            sData.market_buys[bucket_p] += qty;
            sData.last_buy_time[bucket_p] = time;
        }

        // Scaling, CVD, and Tape updates...
        if (sData.market_sells[bucket_p] > sData.max_market_vol) sData.max_market_vol = sData.market_sells[bucket_p];
        if (sData.market_buys[bucket_p] > sData.max_market_vol)  sData.max_market_vol = sData.market_buys[bucket_p];
        sData.running_cvd += delta;
        if (qty > sData.max_tape_qty) sData.max_tape_qty = qty;

        sData.tape.push_front({time, price, qty, is_sell, sData.last_best_bid, sData.last_best_ask});
        if (sData.tape.size() > m_data.m_max_tape_rows) sData.tape.pop_back();

        // Candle update logic...
        double tf = m_data.tick_timeframe;
        double bucket_start = std::floor(time / (tf > 0 ? tf : 1.0)) * (tf > 0 ? tf : 1.0);
        if (sData.candles.empty() || sData.candles.back().time < bucket_start) {
            sData.candles.push_back({bucket_start, price, price, price, price, sData.running_cvd, sData.running_cvd, sData.running_cvd, sData.running_cvd});
        } else {
            TickCandle& cur = sData.candles.back();
            cur.high = std::max(cur.high, price); cur.low = std::min(cur.low, price); cur.close = price;
            cur.cvd_high = std::max(cur.cvd_high, sData.running_cvd); cur.cvd_low = std::min(cur.cvd_low, sData.running_cvd); cur.cvd_close = sData.running_cvd;
        }
        if (sData.candles.size() > 1000) sData.candles.erase(sData.candles.begin());
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
    }
}

void NetworkLayer::process_book_ticker(const std::string& symbol, const nlohmann::json& j)
{
    if (j.find("b") == j.end() || j.find("a") == j.end()) return;

    std::lock_guard<std::recursive_mutex> lock(m_data.mtx);
    SymbolData& sData = m_data.get(symbol);
        
    // b = best bid price, a = best ask price
    sData.last_best_bid = std::stod(j["b"].get<std::string>());
    sData.last_best_ask = std::stod(j["a"].get<std::string>());
}

void NetworkLayer::fetch_dom_snapshot(const std::string& symbol, bool is_futures) 
{
    std::string symUpper = symbol;
    for (auto & c: symUpper) c = toupper(c);

    std::string baseUrl = is_futures ? "https://fapi.binance.com" : "https://api.binance.com";
    std::string endpoint = is_futures ? "/fapi/v1/depth" : "/api/v3/depth";
    std::string url = baseUrl + endpoint + "?symbol=" + symUpper + "&limit=100";

    std::cout << "[REST] Fetching from: " << url << std::endl;

    ix::HttpClient httpClient;
    auto args = std::make_shared<ix::HttpRequestArgs>();
    args->extraHeaders["User-Agent"] = "ORFLterminal/1.0";
    args->followRedirects = true;
    auto response = httpClient.get(url, args);

    if (response->statusCode == 200) 
    {
        try 
        {
            auto j = nlohmann::json::parse(response->body);
            
            std::lock_guard<std::recursive_mutex> lock(m_data.mtx);
            auto& sData = m_data.get(symbol);

            sData.full_asks.clear();
            sData.full_bids.clear();
            sData.last_update_id = j["lastUpdateId"].get<long long>();

            for (auto& a : j["asks"]) 
                sData.full_asks[std::stod(a[0].get<std::string>())] = std::stod(a[1].get<std::string>());
            for (auto& b : j["bids"]) 
                sData.full_bids[std::stod(b[0].get<std::string>())] = std::stod(b[1].get<std::string>());

            sData.snapshot_loaded = true;
            sData.dom_dirty = true;
            std::cout << "[REST] Snapshot loaded for " << symUpper << " at ID " << sData.last_update_id << std::endl;
        } 
        catch (const std::exception& e) 
        {
            std::cerr << "[REST] JSON Error: " << e.what() << std::endl;
        }
    }
    else 
    {
        // This will now print why it failed
        std::cerr << "[REST] FAILED! Status: " << response->statusCode << std::endl;
        std::cerr << "[REST] Error Message: " << response->errorMsg << std::endl;
        if (!response->body.empty())
            std::cerr << "[REST] Payload: " << response->body << std::endl;
    }
}

//  instead of overwriting the book, we update it. If a quantity is "0", we remove the level
void NetworkLayer::process_depth_diff(const std::string& symbol, const nlohmann::json& j) 
{
    std::lock_guard<std::recursive_mutex> lock(m_data.mtx);
    auto& sData = m_data.get(symbol);
    sData.last_update_time = glfwGetTime();
    sData.last_depth_time = GetTimeNow();

    // In Binance Futures, the first event should have u >= U and U <= lastUpdateId+1
    // For simplicity, we just discard any events where the final update ID 'u' 
    // is older than our current snapshot ID.
    long long u = j["u"].get<long long>(); // Final update ID in event
    if (u <= sData.last_update_id) return; 

    // apply Asks
    for (auto& a : j["a"]) 
    {
        double p = std::stod(a[0].get<std::string>());
        double q = std::stod(a[1].get<std::string>());
        if (q == 0.0) 
            sData.full_asks.erase(p);
        else          
            sData.full_asks[p] = q;
    }

    // apply Bids
    for (auto& b : j["b"]) 
    {
        double p = std::stod(b[0].get<std::string>());
        double q = std::stod(b[1].get<std::string>());
        if (q == 0.0) 
            sData.full_bids.erase(p);
        else          
            sData.full_bids[p] = q;
    }

    // update the ID so we know where we are
    sData.last_update_id = u;

    // Pruning (performance)
    // Keep 100 levels on each side so the grouping math remains fast
    while (sData.full_asks.size() > 1000) 
        sData.full_asks.erase(sData.full_asks.begin()); // erase highest ask
    while (sData.full_bids.size() > 1000) 
        sData.full_bids.erase(std::prev(sData.full_bids.end())); // erase lowest bid

    sData.dom_dirty = true; 
}
/* 1 - Storage: std::map with 2,000 entries (1k bid + 1k ask) is roughly 120 KB of data. That is nothing for a modern PC.
*  2 - Aggregation: Your update_content loops through these 2,000 levels to group them.
*       On a modern CPU, iterating 2,000 doubles and doing a floor + map insert takes about 0.1 to 0.3 milliseconds.
*       Since this only happens 10 times per second (because of your dom_dirty flag), the impact on your FPS is zero.
*/

 /*void start(const std::string& symbol, bool isFutures) 
{
    ix::initNetSystem();

    //std::string url = "wss://fstream.binance.com/ws/" + symbol + "@aggTrade";
    //std::string url = "wss://fstream.binance.com/ws/ethusdt@aggTrade";
    //std::string url = "wss://stream.binance.com:9443/ws/ethusdt@aggTrade"; // spot
    //std::string url = "wss://stream.binance.com:9443/ws/ethusdt@kline_1m";
    // Binance allows you to subscribe to many symbols in one URL. The format is:
    // wss://fstream.binance.com/stream?streams=btcusdt@aggTrade/ethusdt@aggTrade/solusdt@aggTrade
    std::string url;
    if (isFutures)
        url = "wss://fstream.binance.com/ws/" + symbol + "@aggTrade";
    else
        url = "wss://stream.binance.com:9443/ws/" + symbol + "@aggTrade";
            
    m_ws.setUrl(url);

    m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) 
        {
            process_json(msg->str);
        }
        else if (msg->type == ix::WebSocketMessageType::Open) 
        {
            std::cout << "Successfully connected to Binance Futures!" << std::endl;
        } 
        else if (msg->type == ix::WebSocketMessageType::Error)
        {
            std::cerr << "Error: " << msg->errorInfo.reason << std::endl;
        }
        else if (msg->type == ix::WebSocketMessageType::Close) 
        {
            std::cout << "Connection Closed." << std::endl;
        }
    });
    m_ws.start();
}*/

/*void process_json(const std::string& raw) 
{
    try 
    {
        // If (is_buyer_maker) is true, a "Maker" was waiting to buy, and a "Taker" hit them. That is a Market Sell.
        // If is false, it is a Market Buy.
        auto j = nlohmann::json::parse(raw);
        double price = std::stod(j["p"].get<std::string>());
        double time  = j["T"].get<double>() / 1000.0;
        double quantity = std::stod(j["q"].get<std::string>());
        bool is_sell = j["m"].get<bool>();

        double tick_delta = is_sell ? -quantity : quantity;

        // Example: if timeframe is 1.0s, 161400.45 becomes 161400.00
        double bucket_start = std::floor(time / m_data.tick_timeframe) * m_data.tick_timeframe;

        std::lock_guard<std::recursive_mutex> lock(m_data.mtx);

        // update the persistent CVD total safely inside the lock
        m_data.running_cvd += tick_delta;

        if (quantity > m_data.max_tape_qty) 
            m_data.max_tape_qty; // Keep track of the "Whale" for scaling

        // update the Tape
        TapeTick tt = {time, price, quantity, is_sell};
        m_data.tape.push_front(tt);
        if (m_data.tape.size() > m_data.max_tape_rows) 
            m_data.tape.pop_back();

        // update candles
        if (m_data.candles.empty() || m_data.candles.back().time < bucket_start) 
        {
            // new candle
            TickCandle new_c;
            new_c.time  = bucket_start;
            new_c.open = new_c.high = new_c.low = new_c.close = price;
            new_c.cvd_open = new_c.cvd_high = new_c.cvd_low = new_c.cvd_close = m_data.running_cvd;
            new_c.buy_vol = !is_sell ? quantity : 0;
            new_c.sell_vol = is_sell ? quantity : 0;
            new_c.delta = tick_delta;
            new_c.cvd = m_data.running_cvd;
            new_c.trade_count = 1;
            m_data.candles.push_back(new_c);
        }
        else
        {
            // update current candle
            TickCandle& cur = m_data.candles.back();
            cur.high  = std::max(cur.high, price);
            cur.low   = std::min(cur.low, price);
            cur.cvd_high = std::max(cur.cvd_high, m_data.running_cvd);
            cur.cvd_low  = std::min(cur.cvd_low, m_data.running_cvd);
            cur.cvd_close = m_data.running_cvd;
            cur.close = price;
            if (is_sell) cur.sell_vol += quantity;
            else cur.buy_vol += quantity;
            cur.delta += tick_delta;
            cur.cvd = m_data.running_cvd;
            cur.trade_count++;
        }

        if (m_data.candles.size() > 500) m_data.candles.erase(m_data.candles.begin());
    }
    catch (const std::exception& e) 
    {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
    }
}*/
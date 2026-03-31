#include <network_layer.h>

NetworkLayer::NetworkLayer(MarketData& data) : m_data(data) 
{}

void NetworkLayer::end()
{
    m_ws.stop();
    ix::uninitNetSystem();
} 

void NetworkLayer::start_multi(const std::set<std::string>& symbols, bool is_futures) 
{
    ix::initNetSystem();
        
    std::string base = is_futures ? 
        "wss://fstream.binance.com/stream?streams=" 
        : "wss://stream.binance.com:9443/stream?streams=";

    std::string stream_path = "";
    for (auto& s : symbols) 
        stream_path += s + "@aggTrade/" + s + "@bookTicker/";

    if (!stream_path.empty()) stream_path.pop_back(); // remove last '/'

    m_ws.setUrl(base + stream_path);

    m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) 
        {
            auto root = nlohmann::json::parse(msg->str);
            // In combined streams, the actual data is in ["data"]
             // The symbol name is in ["stream"] (e.g. "btcusdt@aggTrade")
            std::string stream_name = root["stream"];
            auto& data = root["data"];

            // Route to the correct processor based on stream type
            if (stream_name.find("@bookTicker") != std::string::npos) 
            {
                process_book_ticker(data);
            } 
            else if (stream_name.find("@aggTrade") != std::string::npos) 
            {
                std::string symbol = stream_name.substr(0, stream_name.find('@'));
                process_tick_data(symbol, data);
            }
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
}

void NetworkLayer::process_tick_data(const std::string& symbol, const nlohmann::json& j) 
{
    try 
    {
        double price = std::stod(j["p"].get<std::string>());
        double qty   = std::stod(j["q"].get<std::string>());
        double time  = j["T"].get<double>() / 1000.0;
        bool is_sell = j["m"].get<bool>();
        double delta = is_sell ? -qty : qty;

        std::lock_guard<std::recursive_mutex> lock(m_data.mtx);

        // Access the specific data for THIS symbol
        SymbolData& sData = m_data.get(symbol);
            
        // Update CVD
        sData.running_cvd += delta;

        // Update Max Qty for visual scaling
        if (qty > sData.max_tape_qty) sData.max_tape_qty = qty;

        // Update Tape (Create Tape Entry with BBO Snapshot)
        TapeTick tt;
        tt.time = time;
        tt.price = price;
        tt.quantity = qty;
        tt.is_sell = is_sell;
        tt.bid_at_time = sData.last_best_bid;
        tt.ask_at_time = sData.last_best_ask;
        sData.tape.push_front(tt);
        if (sData.tape.size() > m_data.m_max_tape_rows) 
            sData.tape.pop_back();

        // Update Candles
        double bucket_start = std::floor(time / m_data.tick_timeframe) * m_data.tick_timeframe;

        if (sData.candles.empty() || sData.candles.back().time < bucket_start) 
        {
            TickCandle nc;
            nc.time = bucket_start;
            nc.open = nc.high = nc.low = nc.close = price;
            nc.cvd_open = nc.cvd_high = nc.cvd_low = nc.cvd_close = sData.running_cvd;
            sData.candles.push_back(nc);
        } 
        else 
        {
            TickCandle& cur = sData.candles.back();
            cur.high = std::max(cur.high, price);
            cur.low  = std::min(cur.low, price);
            cur.close = price;
            cur.cvd_high = std::max(cur.cvd_high, sData.running_cvd);
            cur.cvd_low  = std::min(cur.cvd_low, sData.running_cvd);
            cur.cvd_close = sData.running_cvd;
        }

        if (sData.candles.size() > 1000) sData.candles.erase(sData.candles.begin());
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
    }
}

void NetworkLayer::process_book_ticker(const nlohmann::json& j)
{
    std::string symbol = j["s"].get<std::string>();
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::tolower);

    std::lock_guard<std::recursive_mutex> lock(m_data.mtx);
    SymbolData& sData = m_data.get(symbol);
        
    // b = best bid price, a = best ask price
    sData.last_best_bid = std::stod(j["b"].get<std::string>());
    sData.last_best_ask = std::stod(j["a"].get<std::string>());
}

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
#include <iostream>
#include <map>
#include <set>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <list>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <tuple>
#include <format>
#include <map>
#include <set>
#include <deque>
#include <queue>
#include <stack>
#include <limits>
#include <cassert>

enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    Market
};

enum class Side
{
    Buy,
    Sell
};

using Price = std::int32_t; // prices can be negative
using Quantity = std::uint32_t; // unsigned integer
using OrderId = std::uint64_t; // unsigned integer

struct LevelInfo
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos
{
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_{bids}
        , asks_{asks}
        {}

    const LevelInfos& GetBids() const {return bids_;}
    const LevelInfos& GetAsks() const {return asks_;}

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{orderType}
        , orderId_{orderId}
        , side_{side}
        , price_{price}
        , initialQuantity_{quantity}
        , remainingQuantity_{quantity} // to help us tell if its filled or not
        {}

        OrderId GetOrderId() const {return orderId_;}
        Side GetSide() const {return side_;}
        Price GetPrice() const {return price_;}
        OrderType GetOrderType() const {return orderType_;}
        Quantity GetInitialQuantity() const {return initialQuantity_;} // if we want to display inital quantity back to user
        Quantity GetRemainingQuantity() const {return remainingQuantity_;}
        Quantity GetFilledQuantity() const {return GetInitialQuantity() - GetRemainingQuantity();}
        bool IsFilled() const {return GetRemainingQuantity() == 0;}
        // lowest quantity between two orders is the quantity that fills both orders because you can't overfill an order
        void Fill(Quantity quantity)
        {
            if (quantity > GetRemainingQuantity())
                throw std::logic_error(std::format("Order({}) cannot be filled for more than its remaining quantity", GetOrderId()));

            remainingQuantity_ -= quantity;
        }
private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
// uses a list over a vector for simplicity
using OrderPointers = std::list<OrderPointer>;

// add side in modification (optional)

class OrderModify
{
public: 
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_ {orderId}
        , side_ {side}
        , price_ {price}
        , quantity_ {quantity}
    {}

    OrderId GetOrderId() const {return orderId_;}
    Side GetSide() const {return side_;}
    Price GetPrice() const {return price_;}
    Quantity GetQuantity() const {return quantity_;}

    // converting given order into a new order

    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type,GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;

};

 // representation for a matched order 
 // aggregation of trade objects for bid and ask trade info objects

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_{bidTrade}
        , askTrade_{askTrade}
    {}

    const TradeInfo& GetBidTrade() const {return bidTrade_;}
    const TradeInfo& GetAskTrade() const {return askTrade_;}

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class Orderbook
{
private:

    struct OrderEntry
    {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;
    
    bool CanMatch(Side side, Price price) const
    {
        if (side == Side::Buy)
        { 
            if (asks_.empty())
                return false;

            const auto& [bestAsk, _] = *asks_.begin();
                return price >= bestAsk;
        }
        else
        {
            if (bids_.empty())
                return false;

            const auto& [bestBid, _] = *bids_.begin();
                return price <= bestBid;
        }
    }

    Trades MatchOrders()
    {
        Trades trades;
        trades.reserve(orders_.size());
        
        while (true)
        {
            if (bids_.empty() || asks_.empty())
                break;
            
            auto& [bidPrice, bids] = *bids_.begin();
            auto& [askPrice, asks] = *asks_.begin();

            if (bidPrice < askPrice)
                break;
            
            while (bids.size() && asks.size())
            {
                auto bid = bids.front();
                auto ask = asks.front();

                Quantity quantity = std::min(bid -> GetRemainingQuantity(), ask -> GetRemainingQuantity());

                bid->Fill(quantity);
                ask->Fill(quantity);

                if (bid->IsFilled())
                {
                    bids.pop_front();
                    orders_.erase(bid->GetOrderId());
                }

                if (ask->IsFilled())
                {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }

                trades.push_back(Trade{ 
                    TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
                    TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity} // quantity can be part of constructor for trade object because its the same for both bid/ask

                });

            }


            if (bids.empty())
                bids_.erase(bidPrice);
                
            if (asks.empty())
                asks_.erase(askPrice);
        }

        if (!bids_.empty())
        {
            auto& [_, bids] = *bids_.begin();
            auto& order = bids.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }

        if (!asks_.empty())
        {
            auto& [_, asks] = *asks_.begin();
            auto& order = asks.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
        return trades;
    }

public:

    Trades AddOrder(OrderPointer order) 
    {
        // address exit conditions first
        if (orders_.contains(order->GetOrderId()))
            return {};

        if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            return {};

        OrderPointers::iterator iterator;

        if (order->GetSide() == Side::Buy)
        {
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::prev(orders.end());
        }
        else
        {
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::prev(orders.end());
        }

        orders_.insert({order->GetOrderId(), OrderEntry{ order,iterator}});
        return MatchOrders();
    }
    
    void CancelOrder(OrderId orderId)
    {
        if (!orders_.contains(orderId))
            return;
        
        const auto [order, iterator] = orders_.at(orderId);
        orders_.erase(orderId);

        if (order->GetSide() == Side::Sell)
        {
            auto price = order->GetPrice();
            auto& orders = asks_.at(price);
            orders.erase(iterator);
            if (orders.empty())
                asks_.erase(price);
        }
        else
        {
            auto price = order->GetPrice();
            auto& orders = bids_.at(price);
            orders.erase(iterator);
            if (orders.empty())
                bids_.erase(price);
        }
    }

    Trades MatchOrder(OrderModify order)
    {
        if(!orders_.contains(order.GetOrderId()))
            return {};
        
        const auto [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const {return orders_.size();}

    OrderbookLevelInfos GetOrderInfos() const
    {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
        {
            return LevelInfo(price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](Quantity runningSum, const OrderPointer& order)
            {
                return runningSum + order->GetRemainingQuantity();
            }));
        };

        for (const auto& [price, orders] : bids_)
            bidInfos.push_back(CreateLevelInfos(price, orders));

        for (const auto& [price, orders] : asks_)
            askInfos.push_back(CreateLevelInfos(price, orders));

        return OrderbookLevelInfos{ bidInfos, askInfos };
    }

    const Order* GetOrder(OrderId orderId) const
    {
        if (!orders_.contains(orderId))
            return nullptr;

        return orders_.at(orderId).order_.get();
    }

};


void PrintBook(const Orderbook& orderbook)
{
    const auto& orderInfos = orderbook.GetOrderInfos();

    std::cout << "\n ===== Orderbook ===== \n Price | Quantity" << std::endl;

    std::cout << "---- Asks ----" << std::endl;
    for(const auto& level: orderInfos.GetAsks())
    {
        std::cout << level.price_ << "|" << level.quantity_ << std::endl;
    }

    std::cout << "---- Bids ----" << std::endl;
    for(const auto& level: orderInfos.GetBids())
    {
        std::cout << level.price_ << "|" << level.quantity_ << std::endl;
    }
}


void TestPartialFill()
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));
    //PrintBook(orderbook);
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 100, 4));
    //PrintBook(orderbook);

    assert(orderbook.Size() == 1); // buy order still resting, partially filled
    std::cout << "\nTestPartialFill passed\n";
}

void TestFullFill()
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));
    //PrintBook(orderbook);
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 100, 10));
    //PrintBook(orderbook); // should show full fill

    assert(orderbook.Size() == 0); // order fully filled and removed
    std::cout << "\nTestFullFill passed\n";

}

void TestFIFO()
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Buy, 100, 10));

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 3, Side::Sell, 100, 10));

    //PrintBook(orderbook);

    // order 1 should be matched first, order 2 should still be fully resting at 10
    assert(orderbook.Size() == 1);
    assert(orderbook.GetOrder(1) == nullptr);
    assert(orderbook.GetOrder(2) != nullptr);
    assert(orderbook.GetOrder(2) -> GetRemainingQuantity() == 10);

    std::cout << "\nTestFIFO passed\n";
}

void TestCancel()
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));
    assert(orderbook.Size() == 1);

    orderbook.CancelOrder(1);
    assert(orderbook.Size() == 0);

    std::cout << "\nTestCancel passed\n";
}

void TestPriority()
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Buy, 100, 10));

    PrintBook(orderbook);

    orderbook.MatchOrder(OrderModify(1, Side::Buy, 100, 5)); // change the quantity of order 1, same price, so it should still have the same priority

    PrintBook(orderbook);

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 3, Side::Sell, 100, 5));
    std::cout << (orderbook.GetOrder(1) ? "Order 1 exists" : "Order 1 does not exist") << std::endl;
    std::cout << (orderbook.GetOrder(2) ? "Order 2 exists" : "Order 2 does not exist") << std::endl;

    // order 1 should not exist because it should have been filled completely

}

int main() 
{
    
    TestPartialFill();
    TestFullFill();
    TestFIFO();
    TestCancel();
    TestPriority();
    return 0;
}

/**
 * to add:
 * self-trade prevention
 * market orders
 * fillorkill order
 * measure latency
 */

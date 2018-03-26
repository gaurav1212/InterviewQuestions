#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <bitset>
#include <cstdio>
#include <limits>
#include <vector>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
using namespace std;

enum ORDERTYPE { IOC, GFD };
enum COMMANDS { BUY = 0, SELL, CANCEL, MODIFY, PRINT };

struct OrderDetails
{
    COMMANDS _eType;        // entry type : buy or sell
    ORDERTYPE _oType;       // order type IOC or GFD
    unsigned int _price;
    unsigned int _quantity; // remaining quantity at price
    bool _canceledOrTraded;
    unsigned int _index;    // time-substitute 
    OrderDetails(COMMANDS eType,
        ORDERTYPE oType,
        unsigned int price,
        unsigned int quantity,
        unsigned int index)
    {
        _eType = eType;
        _oType = oType;
        _price = price;
        _quantity = quantity;
        _canceledOrTraded = false;
        _index = index;
    };
    OrderDetails() {};
};

class TradingEngine
{
private:

    // A map to store order details from order ID
    unordered_map<string, OrderDetails> m_orderDetails;

    // commands in string
    const vector<string> m_cmdStr{ "BUY", "SELL", "CANCEL", "MODIFY", "PRINT" };

    // string to Enum converter
    unordered_map<string, COMMANDS> m_cmdToEnum;

    // A tree for sell orders, key = price (STL maps are trees)
    // with each price we map a list of orderIds in chronological order
    map<unsigned int, list<string>> m_sellsTree;

    // A tree for buy orders, key = price (STL maps are trees)
    // with each price we map a list of orderIds in chronological order
    map<unsigned int, list<string>> m_buysTree;

    // This variable indicates that all trade checks are done, and no new 
    // entry is added since last trades
    bool m_tradeCheckDone = true;

    unsigned int m_timeIndex = 0;

    // return 0 if parsing proceeded successfully
    // else non-zero
    //
    int ParseOrderType(string& orderType, ORDERTYPE& type)
    {
        if (orderType == "IOC" || orderType == "GFD")
        {
            type = orderType == "IOC" ? IOC : GFD;
            return 0;
        }
        return 1;
    };

    int ParseBuySell(string& buySell, COMMANDS& cmd)
    {
        if (buySell == "BUY" || buySell == "SELL")
        {
            cmd = buySell == "BUY" ? BUY : SELL;
            return 0;
        }
        return 1;
    };

    // Print the prices and quantities, for buy and sell
    // 
    void PrintStatus()
    {
        CheckAndDoTrade();
        string msgs[] = { "SELL:", "BUY:" };

        for (int i = 0; i < 2; i++)
        {
            cout << msgs[i] << endl;
            const auto &tree = i == 0 ? move(m_sellsTree) : move(m_buysTree);
            PrintTreePriceQtys(tree);
        }
    };

    // Print a tuple of price and quantities per line for a buy or sell tree 
    //
    void PrintTreePriceQtys(const map<unsigned int, list<string>>& tree)
    {
        for (auto it = tree.rbegin(); it != tree.rend(); it++)
        {
            int qty = 0;
            for (auto id : it->second)
            {
                auto detailsIt = m_orderDetails.find(id);
                if (detailsIt != m_orderDetails.end())
                {
                    qty += detailsIt->second._quantity;
                }
            }
            cout << it->first << " " << qty << endl;
        }
    }

    // Check if a trade is possible 
    //
    void CheckAndDoTrade()
    {
        if (m_tradeCheckDone)
        {
            return;
        }

        // pick the smallest sell bid and largest buy bid .. trade them
        // keep trading till it is not possible to trade anymore
        //
        while (1)
        {
            auto sit = m_sellsTree.begin();
            auto bit = m_buysTree.rbegin();

            if (m_sellsTree.empty() || m_buysTree.empty() ||
                sit->first > bit->first)
            {
                break;
            }

            PrintAndTradeAPair(sit->second.front(), bit->second.front());
        }

        m_tradeCheckDone = true;
    };

    // Orders can be cancelled if they are not traded or previously cancelled
    //  
    void CancelOrder(const string& orderId)
    {
        auto detailsIt = m_orderDetails.find(orderId); 
        if (detailsIt != m_orderDetails.end())
        {
            auto& details = detailsIt->second;
            if (details._canceledOrTraded)
            {
                return;
            }

            details._quantity = 0;
            details._canceledOrTraded = true;
            RemoveFromTrees(orderId);
        }
    };

    // for IOC we need to trade now .. make the best possible trade
    // ... better to have some than none
    //
    void TradeIOCOrder(const string& orderId)
    {
        auto detailsIt = m_orderDetails.find(orderId); 
        if (detailsIt != m_orderDetails.end())
        {
            auto& orderDetails = detailsIt->second; 
            auto qty = orderDetails._quantity;
            auto price = orderDetails._price;
            if (orderDetails._eType == BUY)
            {
                // check smallest in sell tree
                while (qty > 0)
                {
                    auto it = m_sellsTree.begin();

                    if (it == m_sellsTree.end() || it->first > price)
                    {
                        break;
                    }

                    qty -= PrintAndTradeAPair(it->second.front(), orderId);
                }
            }
            else
            {
                // check largest in buy tree
                while (qty > 0)
                {
                    auto it = m_buysTree.rbegin();

                    if (it == m_buysTree.rend() || it->first < price)
                    {
                        break;
                    }

                    qty -= PrintAndTradeAPair(it->second.front(), orderId);
                }
            }
            m_orderDetails[orderId]._canceledOrTraded = true;
            RemoveFromTrees(orderId);
        }

    };

    //  Do a trading b/w orderID1 and orderID2 
    //  Returns the number of traded stocks
    //
    int PrintAndTradeAPair(string orderId1, string orderId2)
    {
        OrderDetails& details1 = m_orderDetails[orderId1];
        OrderDetails& details2 = m_orderDetails[orderId2];
        if (details1._index > details2._index)
        {
            PrintAndTradeAPair(orderId2, orderId1);
        }
        unsigned int qty = min(details1._quantity, details2._quantity);

        if (qty > 0)
        {
            cout << "TRADE " << orderId1 << " " << details1._price << " " <<
                qty << " " << orderId2 << " " << details2._price << " " << qty << endl;

            details1._quantity -= qty;
            details2._quantity -= qty;
        }


        if (details1._quantity == 0)
        {
            RemoveFromTrees(orderId1);
        }

        if (details2._quantity == 0)
        {
            RemoveFromTrees(orderId2);
        }

        details1._canceledOrTraded = true;
        details2._canceledOrTraded = true;

        return qty;
    }

    // Remove an order from buy and sell tree
    //
    void RemoveFromTrees(const string& orderId)
    {
        auto detailsIt = m_orderDetails.find(orderId); 
        if (detailsIt != m_orderDetails.end())
        {
            auto & details = detailsIt->second;
            auto & tree = details._eType == BUY ? m_buysTree : m_sellsTree;
            auto treesIt = tree.find(details._price);
            if (treesIt != tree.end())
            {
                auto & ordersList = treesIt->second;
                RemoveOrderIdFromList(ordersList, orderId);
                if (ordersList.empty())
                {
                    tree.erase(details._price);
                }
            }
        }
    }

    // Remove a string value from list<string>, optimized to 
    // remove front and back element (most common case)
    // (Note that: In vector, removing from front is very 
    //  inefficient, not with list)
    //
    void RemoveOrderIdFromList(list<string>& lst, const string& val)
    {
        if (lst.front() == val)
        {
            lst.pop_front();
            return;
        }

        if (lst.back() == val)
        {
            lst.pop_back();
            return;
        }

        list<string> right;  // part after val
        while (!lst.empty())
        {
            if (lst.back() == val)
            {
                lst.pop_back();
                break;
            }

            right.push_front(lst.back());
            lst.pop_back();
        }
        lst.merge(right);
    }

    // Modify an orderID fields, change buy or sell tree if needed
    // An order can not be modified if it is traded or cancelled
    //
    void ModifyOrder(string orderId, COMMANDS buySellCmdType,
        unsigned int price, unsigned int quantity)
    {
        auto detailsIt = m_orderDetails.find(orderId); 
        if (detailsIt != m_orderDetails.end())
        {
            auto & details = detailsIt->second;
            if (details._canceledOrTraded)
            {
                return;
            }

            // Since we are printing trades as soon as some are available
            // a modify operation can also trigger trade check
            bool CheckAndDoTradeAfter = details._eType != buySellCmdType;

            // Remove -> modify -> insert
            RemoveFromTrees(orderId);

            details._eType = buySellCmdType;
            details._price = price;
            details._quantity = quantity;
            details._index = m_timeIndex++;

            InsertIntoBuySellTrees(orderId);

            if (CheckAndDoTradeAfter)
                CheckAndDoTrade();
        }
    }
    
    void ProcessBuySellOrder(COMMANDS& cmd,
        ORDERTYPE& type,
        unsigned int price,
        unsigned int quantity,
        string& orderId)
    {
        m_orderDetails[orderId] = OrderDetails(cmd, type, price,
            quantity, m_timeIndex++);
        
        InsertIntoBuySellTrees(orderId);

        // If trade type is IOC, we need to make the trade 
        if (type == IOC)
        {
            TradeIOCOrder(orderId);
        }

        // It is inefficient to trade after each buy or sell 
        // order.. we can hold till end of day (destructor) 
        // is called.
        // But as the problem "states" .. we will check after 
        // each buy and sell order
        CheckAndDoTrade();
    }

    // Insert an order into buy tree and sell price tree
    //
    void InsertIntoBuySellTrees(string orderId)
    {
        m_tradeCheckDone = false;
        auto detailsIt = m_orderDetails.find(orderId); 
        if (detailsIt != m_orderDetails.end())
        {
            auto& details = detailsIt->second;
            auto& tree = details._eType == BUY ? m_buysTree : m_sellsTree;
            auto treeIt = tree.find(details._price);
            if (treeIt != tree.end())
            {
                treeIt->second.push_back(orderId);
            }
            else
            {
                tree[details._price] = list<string>{ orderId };
            }
        }
    }

    vector<string> SplitCmdLineArgs(string& cmdArgs)
    {
        vector<string> cmdList;
        stringstream ss(cmdArgs);
        string arg;
        while (getline(ss, arg, ' '))
        {
            if (!arg.empty())
            {
                cmdList.push_back(arg);
            }    
        }
        return cmdList;
    }

public:

    TradingEngine()
    {
        for (unsigned int i = 0; i < m_cmdStr.size(); i++)
        {
            m_cmdToEnum[m_cmdStr[i]] = (COMMANDS)i;
        }
    }

    ~TradingEngine()
    {
        // End of the day trading, it will be efficient
        // to skip trading till very end to maximize profit
        // THIS IS NOT WHAT PROBLEM STATES!!
        // (according to problem, we trade as soon any trade 
        // is possible.)
        // This call to CheckAndDoTrade() does nothing. As we check and
        // do trade after each buy and sell order. 
        CheckAndDoTrade();
    }

    // return 0 if parsing proceeded successfully
    // else non-zero
    //
    int ParseNextCommand(string cmd)
    {
        string cmdArgs;
        vector<string> cmdArgsList;
        string orderType;
        ORDERTYPE type;
        int price;
        int quantity;
        string orderId;
        string buySell;
        COMMANDS buySellCmd;

        auto cmdIt = m_cmdToEnum.find(cmd);
        if (cmdIt == m_cmdToEnum.end())
        {
            return 1;
        }
        
        // get the rest of the line
        getline(cin, cmdArgs);
        cmdArgsList = SplitCmdLineArgs(cmdArgs);
        auto& cmdEnum = cmdIt->second;
        
        switch (cmdEnum)
        {
          case BUY:
          case SELL:
            // to catch string to int conversion exceptions
            try
            {
                // there should be 4 args .. else malformed command
                if (cmdArgsList.size() == 4)
                {
                    orderType = cmdArgsList[0];
                    price = atoi(cmdArgsList[1].c_str());
                    quantity = atoi(cmdArgsList[2].c_str());
                    orderId = cmdArgsList[3];
                    if (!ParseOrderType(orderType, type) && price > 0 &&
                        quantity > 0)
                    {
                        ProcessBuySellOrder(cmdEnum, type, price, quantity, 
                            orderId);
                    }
                }
            }
            catch(exception ex) { }
            break;
          case CANCEL:
            {  
              // there should be 1 arg .. else malformed command line
              if (cmdArgsList.size() == 1)
              {
                  orderId = cmdArgsList[0];
                  CancelOrder(orderId);
              }
            }
            break;
          case MODIFY:
            // to catch string to int conversion exceptions
            try 
            {   
                // there should be 4 args .. else malformed command
                if (cmdArgsList.size() == 4)
                {
                    orderId = cmdArgsList[0];
                    buySell = cmdArgsList[1];
                    price = atoi(cmdArgsList[2].c_str());
                    quantity = atoi(cmdArgsList[3].c_str());

                    if (!ParseBuySell(buySell, buySellCmd) && price > 0 &&
                        quantity > 0)
                    {
                        ModifyOrder(orderId, buySellCmd, price, quantity);
                    }
                }
            }
            catch(exception ex) { }
            break;
          case PRINT:
            // there should be no command line arg .. else malformed cmd
            if (cmdArgsList.empty())
            {
                PrintStatus();
            } 
            break;
          default:
            return 1;
        }

        return 0;
    };
};

int main() {

    TradingEngine engine;
    string cmd;
    while (cin >> cmd)
    {
        // skip line if unable to parse
        //
        if (engine.ParseNextCommand(cmd))
        {
            getline(cin, cmd);
        }
    }
    return 0;
}
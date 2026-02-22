/* Copyright (C) 2025 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#pragma once

#include "TestCppClient.h"

#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include "command.h"
#include "event.h"

class IbkrClient : public TestCppClient
{
public:
	IbkrClient(const std::string& host = "127.0.0.1", int port = 7497, int clientId = 0);
	~IbkrClient();

	bool connect(const char* host, int port, int clientId = 0);
	void disconnect() const;
	bool isConnected() const;
	void setConnectOptions(const std::string&);
	void setOptionalCapabilities(const std::string&);
	void processMessages();
	void processLoop();
	void stop();
	void start();
	void pushCommand(Command command);
	void pushEvent(Event event);
	void processCommands();
	std::queue<Event> consumeEvents();

	void getHistoricalTest();
	void scanTest();
	void scanTest1();
	void reqMarketDataTest();

	std::unordered_map<TickerId, double> m_last;
	std::unordered_map<TickerId, double> m_close;

	// EWrapper overrides with custom logic
	void connectAck() override;
	void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attribs) override;
	void tickSize(TickerId tickerId, TickType field, Decimal size) override;
	void historicalData(TickerId reqId, const Bar& bar) override;
	void historicalDataEnd(int reqId, const std::string& startDateStr, const std::string& endDateStr) override;
	void scannerParameters(const std::string& xml) override;
	void scannerData(int reqId, int rank, const ContractDetails& contractDetails,
		const std::string& distance, const std::string& benchmark,
		const std::string& projection, const std::string& legsStr) override;
	void scannerDataEnd(int reqId) override;
	void updateAccountValue(const std::string& key, const std::string& val,
		const std::string& currency, const std::string& accountName) override;
	void updatePortfolio(const Contract& contract, Decimal position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const std::string& accountName) override;
	void position(const std::string& account, const Contract& contract,
		Decimal position, double avgCost) override;

private:
	std::string m_host;
	int m_port;
	int m_clientId;
	std::mutex m_commandMutex;
	std::mutex m_eventMutex;
	std::queue<Command> m_commandQueue;
	std::queue<Event> m_eventQueue;
	std::unordered_map<int, std::vector<ScannerResultItem>> m_pendingScannerResults;
	std::unordered_map<int, std::vector<CandleData>> m_pendingHistoricalData;
	std::unordered_map<int, std::string> m_reqIdToSymbol;

	void saveScannerXML(const std::string& xml);

	// Own socket — TestCppClient's socket members are private and never connected
	EReaderOSSignal m_osSignal;
	EClientSocket* const m_pClient;
	std::unique_ptr<EReader> m_pReader;
	bool m_extraAuth;
};


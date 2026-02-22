/* Copyright (C) 2025 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "StdAfx.h"

#include "ibkr.h"

#include "EClientSocket.h"
#include "EPosixClientSocketPlatform.h"

#include "Contract.h"
#include "ScannerSubscription.h"
#include "ScannerSubscriptionSamples.h"
#include "CommonDefs.h"

#include <stdio.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>
#include <cstdint>
#include <cerrno>
#include <queue>

///////////////////////////////////////////////////////////
// member funcs
//! [socket_init]
IbkrClient::IbkrClient(const std::string& host, int port, int clientId) :
	m_host(host)
	, m_port(port)
	, m_clientId(clientId)
	, m_osSignal(2000)
	, m_pClient(new EClientSocket(this, &m_osSignal))
	, m_extraAuth(false)
{
}
//! [socket_init]

IbkrClient::~IbkrClient()
{
	if (m_pReader)
		m_pReader.reset();
	delete m_pClient;
}

void IbkrClient::getHistoricalTest() {
	std::time_t rawtime;
	std::tm timeinfo;
	char queryTime[80];

	std::time(&rawtime);
#if defined(IB_WIN32)
	gmtime_s(&timeinfo, &rawtime);
#else
	gmtime_r(&rawtime, &timeinfo);
#endif
	std::strftime(queryTime, sizeof queryTime, "%Y%m%d-%H:%M:%S", &timeinfo);
	Contract contract;
	contract.symbol = "BE";
	contract.secType = "STK";
	contract.currency = "USD";
	contract.exchange = "SMART";

	m_pClient->reqHistoricalData(4001, contract, queryTime, "1 M", "1 day", "MIDPOINT", 1, 1, false, TagValueListSPtr());
	std::this_thread::sleep_for(std::chrono::seconds(2));
	m_pClient->cancelHistoricalData(4001);
}

std::queue<Event> IbkrClient::consumeEvents() {
	std::queue<Event> localQueue;
	{
		std::lock_guard<std::mutex> lock(m_eventMutex);
		std::swap(localQueue, m_eventQueue);
	}
	return localQueue;
}

void IbkrClient::scanTest() {
	m_pClient->reqScannerParameters();
	std::this_thread::sleep_for(std::chrono::seconds(2));

	m_pClient->reqScannerSubscription(7001, ScannerSubscriptionSamples::HotUSStkByVolume(), TagValueListSPtr(), TagValueListSPtr());

	TagValueSPtr t1(new TagValue("usdMarketCapAbove", "10000"));
	TagValueSPtr t2(new TagValue("optVolumeAbove", "1000"));
	TagValueSPtr t3(new TagValue("usdMarketCapAbove", "100000000"));

	TagValueListSPtr TagValues(new TagValueList());
	TagValues->push_back(t1);
	TagValues->push_back(t2);
	TagValues->push_back(t3);

	m_pClient->reqScannerSubscription(7002, ScannerSubscriptionSamples::HotUSStkByVolume(), TagValueListSPtr(), TagValues);

	TagValueSPtr t(new TagValue("underConID", "265598"));
	TagValueListSPtr AAPLConIDTag(new TagValueList());
	AAPLConIDTag->push_back(t);
	m_pClient->reqScannerSubscription(7003, ScannerSubscriptionSamples::ComplexOrdersAndTrades(), TagValueListSPtr(), AAPLConIDTag);

	std::this_thread::sleep_for(std::chrono::seconds(2));
	m_pClient->cancelScannerSubscription(7001);
	m_pClient->cancelScannerSubscription(7002);
}

void IbkrClient::reqMarketDataTest() {
	Contract contract;
	contract.symbol = "CART";
	contract.secType = "STK";
	contract.exchange = "NASDAQ";
	m_pClient->reqMktData(8001, contract, "", false, false, TagValueListSPtr());
}

void IbkrClient::scanTest1() {
	ScannerSubscription scanSub;
	scanSub.instrument = "STK";
	scanSub.locationCode = "STK.US";
	scanSub.scanCode = "TOP_AFTER_HOURS_PERC_GAIN";

	TagValueListSPtr filters(new TagValueList());
	filters->push_back(TagValueSPtr(new TagValue("priceAbove", "5")));

	m_pClient->reqScannerSubscription(7002, scanSub, TagValueListSPtr(), filters);
}

void IbkrClient::pushCommand(Command command)
{
	std::lock_guard<std::mutex> lock(m_commandMutex);
	m_commandQueue.push(std::move(command));
}

void IbkrClient::pushEvent(Event event) {
	m_eventQueue.push(std::move(event));
}

void IbkrClient::processCommands() {
	std::queue<Command> localQueue;
	{
		std::lock_guard<std::mutex> lock(m_commandMutex);
		std::swap(localQueue, m_commandQueue);
	}

	while (!localQueue.empty()) {
		Command cmd = std::move(localQueue.front());
		localQueue.pop();

		std::visit([this](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, StartScannerCommand>) {
				printf("Processing StartScannerCommand: reqId=%d, scanCode=%s, priceAbove=%.2f\n",
					arg.reqId, arg.scanCode.c_str(), arg.priceAbove);

				ScannerSubscription scanSub;
				scanSub.instrument = "STK";
				scanSub.locationCode = "STK.US";
				scanSub.scanCode = "TOP_AFTER_HOURS_PERC_GAIN";

				TagValueListSPtr filters(new TagValueList());
				filters->push_back(TagValueSPtr(new TagValue("priceAbove", "5")));

				m_pClient->reqScannerSubscription(7002, scanSub, TagValueListSPtr(), filters);
			}
			else if constexpr (std::is_same_v<T, CancelScannerCommand>) {
				printf("Processing CancelScannerCommand: reqId=%d\n", arg.reqId);
				m_pClient->cancelScannerSubscription(arg.reqId);
			}
			else if constexpr (std::is_same_v<T, RequestHistoricalDataCommand>) {
				printf("Processing RequestHistoricalDataCommand: reqId=%d, symbol=%s, duration=%s, barSize=%s\n",
					arg.reqId, arg.symbol.c_str(), arg.durationStr.c_str(), arg.barSizeSetting.c_str());

				m_reqIdToSymbol[arg.reqId] = arg.symbol;

				Contract contract;
				contract.symbol = arg.symbol;
				contract.secType = "STK";
				contract.currency = "USD";
				contract.exchange = "SMART";

				m_pClient->reqHistoricalData(arg.reqId, contract, arg.endDateTime,
					arg.durationStr, arg.barSizeSetting, arg.whatToShow,
					arg.useRTH, 1, false, TagValueListSPtr());
			}
			else if constexpr (std::is_same_v<T, DiscounnectCommand>) {
				printf("Processing DisconnectCommand\n");
				m_pClient->eDisconnect();
			}
			else if constexpr (std::is_same_v<T, RequestAccountDataCommand>) {
				printf("Processing RequestAccountDataCommand: accountCode=%s\n",
					arg.accountCode.c_str());

				if (!arg.accountCode.empty()) {
					m_pClient->reqAccountUpdates(true, arg.accountCode);
				} else {
					m_pClient->reqPositions();
				}
			}
			}, cmd);
	}
}

void IbkrClient::processLoop() {
	printf("Connecting to %s:%d clientId:%d\n", m_host.c_str(), m_port, m_clientId);

	bool bRes = m_pClient->eConnect(m_host.c_str(), m_port, m_clientId, m_extraAuth);
	if (bRes) {
		printf("Connected to %s:%d clientId:%d serverVersion: %d\n",
			m_pClient->host().c_str(), m_pClient->port(), m_clientId, m_pClient->EClient::serverVersion());
		m_pReader = std::unique_ptr<EReader>(new EReader(m_pClient, &m_osSignal));
		m_pReader->start();
	}
	else {
		printf("Cannot connect to %s:%d clientId:%d\n",
			m_pClient->host().c_str(), m_pClient->port(), m_clientId);
		return;
	}

	int counter = 0;
	while (m_pClient->isConnected()) {
		processCommands();
		counter++;
		m_osSignal.waitForSignal();
		errno = 0;
		m_pReader->processMsgs();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::printf("Processed message cycle %d\n", counter);
	}
}

void IbkrClient::stop() {}

void IbkrClient::start() {}

bool IbkrClient::connect(const char* host, int port, int clientId)
{
	printf("Connecting to %s:%d clientId:%d\n", !(host && *host) ? "127.0.0.1" : host, port, clientId);

	bool bRes = m_pClient->eConnect(host, port, clientId, m_extraAuth);

	if (bRes) {
		printf("Connected to %s:%d clientId:%d serverVersion: %d\n",
			m_pClient->host().c_str(), m_pClient->port(), clientId, m_pClient->EClient::serverVersion());
		m_pReader = std::unique_ptr<EReader>(new EReader(m_pClient, &m_osSignal));
		m_pReader->start();
	}
	else
		printf("Cannot connect to %s:%d clientId:%d\n", m_pClient->host().c_str(), m_pClient->port(), clientId);

	return bRes;
}

void IbkrClient::disconnect() const
{
	m_pClient->eDisconnect();
	printf("Disconnected\n");
}

bool IbkrClient::isConnected() const
{
	return m_pClient->isConnected();
}

void IbkrClient::setConnectOptions(const std::string& connectOptions)
{
	m_pClient->setConnectOptions(connectOptions);
}

void IbkrClient::setOptionalCapabilities(const std::string& optionalCapabilities)
{
	m_pClient->setOptionalCapabilities(optionalCapabilities);
}

void IbkrClient::processMessages()
{
	if (!m_pReader) return;
	processCommands();
	m_osSignal.waitForSignal();
	errno = 0;
	m_pReader->processMsgs();
}

//! [connectack]
void IbkrClient::connectAck() {
	if (!m_extraAuth && m_pClient->asyncEConnect())
		m_pClient->startApi();
}
//! [connectack]

// New [tickprice]
void IbkrClient::tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attribs)
{
	switch (field)
	{
	case LAST:
	case DELAYED_LAST:
		m_last[tickerId] = price;
		printf("LAST (%ld): %.2f\n", tickerId, price);
		break;
	case CLOSE:
	case DELAYED_CLOSE:
		m_close[tickerId] = price;
		printf("CLOSE (%ld): %.2f\n", tickerId, price);
		break;
	case BID:
	case DELAYED_BID:
		printf("BID (%ld): %.2f\n", tickerId, price);
		break;
	case ASK:
	case DELAYED_ASK:
		printf("ASK (%ld): %.2f\n", tickerId, price);
		break;
	case HIGH:
	case DELAYED_HIGH:
		printf("HIGH (%ld): %.2f\n", tickerId, price);
		break;
	case LOW:
	case DELAYED_LOW:
		printf("LOW (%ld): %.2f\n", tickerId, price);
		break;
	default:
		return;
	}

	if (m_last.count(tickerId) && m_close.count(tickerId) && m_close[tickerId] != 0)
	{
		double gain = (m_last[tickerId] - m_close[tickerId]) / m_close[tickerId] * 100.0;
		printf(">>> Ticker %ld Gain%%: %.2f%%\n\n", tickerId, gain);
	}
}
// New [ticksize]
void IbkrClient::tickSize(TickerId tickerId, TickType field, Decimal size)
{
	switch (field)
	{
	case VOLUME:
	case DELAYED_VOLUME:
		printf("VOLUME (%ld): %s\n", tickerId, DecimalFunctions::decimalStringToDisplay(size).c_str());
		break;
	case BID_SIZE:
	case DELAYED_BID_SIZE:
		printf("BID_SIZE (%ld): %s\n", tickerId, DecimalFunctions::decimalStringToDisplay(size).c_str());
		break;
	case ASK_SIZE:
	case DELAYED_ASK_SIZE:
		printf("ASK_SIZE (%ld): %s\n", tickerId, DecimalFunctions::decimalStringToDisplay(size).c_str());
		break;
	default:
		return;
	}
}

//! [historicaldata]
void IbkrClient::historicalData(TickerId reqId, const Bar& bar) {
	printf("HistoricalData. ReqId: %ld - Date: %s, Open: %g, High: %g, Low: %g, Close: %g, Volume: %s\n",
		reqId, bar.time.c_str(),
		bar.open, bar.high, bar.low, bar.close,
		DecimalFunctions::decimalStringToDisplay(bar.volume).c_str());

	CandleData candle;
	candle.date = bar.time;
	candle.open = bar.open;
	candle.high = bar.high;
	candle.low = bar.low;
	candle.close = bar.close;

	std::string volumeStr = DecimalFunctions::decimalStringToDisplay(bar.volume);
	try {
		candle.volume = std::stol(volumeStr);
	} catch (...) {
		candle.volume = 0;
	}

	m_pendingHistoricalData[reqId].push_back(candle);
}
//! [historicaldata]

//! [historicaldataend]
void IbkrClient::historicalDataEnd(int reqId, const std::string& startDateStr, const std::string& endDateStr) {
	printf("HistoricalDataEnd. ReqId: %d - Start Date: %s, End Date: %s\n",
		reqId, startDateStr.c_str(), endDateStr.c_str());

	std::vector<CandleData> candles;
	auto dataIt = m_pendingHistoricalData.find(reqId);
	if (dataIt != m_pendingHistoricalData.end()) {
		candles = std::move(dataIt->second);
		m_pendingHistoricalData.erase(dataIt);
	}

	std::string symbol;
	auto symbolIt = m_reqIdToSymbol.find(reqId);
	if (symbolIt != m_reqIdToSymbol.end()) {
		symbol = symbolIt->second;
		m_reqIdToSymbol.erase(symbolIt);
	}

	if (!candles.empty() && !symbol.empty()) {
		printf("Pushing HistoricalDataEvent: symbol=%s, bars=%zu\n", symbol.c_str(), candles.size());

		HistoricalDataEvent evt;
		evt.reqId = reqId;
		evt.symbol = symbol;
		evt.candles = std::move(candles);

		pushEvent(Event{ evt });
	}
}
//! [historicaldataend]

void IbkrClient::saveScannerXML(const std::string& xml)
{
	std::ofstream file("scanner_parameters.xml");
	if (file.is_open()) {
		file << xml;
		file.close();
		std::cout << "Scanner parameters saved.\n";
	}
	else {
		std::cerr << "Failed to open file.\n";
	}
}

//! [scannerparameters]
void IbkrClient::scannerParameters(const std::string& xml) {
	printf("ScannerParameters. %s\n", xml.c_str());
	saveScannerXML(xml);
}
//! [scannerparameters]

//! [scannerdata]
void IbkrClient::scannerData(int reqId, int rank, const ContractDetails& contractDetails,
	const std::string& distance, const std::string& benchmark, const std::string& projection,
	const std::string& legsStr) {
	ScannerResultItem item;
	item.rank = rank;
	item.symbol = contractDetails.contract.symbol;
	item.secType = contractDetails.contract.secType;
	item.currency = contractDetails.contract.currency;
	item.conId = contractDetails.contract.conId;

	m_pendingScannerResults[reqId].push_back(item);
}
//! [scannerdata]

//! [scannerdataend]
void IbkrClient::scannerDataEnd(int reqId) {
	printf("ScannerDataEnd. %d\n", reqId);
	std::vector<ScannerResultItem> results;
	{
		auto it = m_pendingScannerResults.find(reqId);
		if (it != m_pendingScannerResults.end()) {
			results = std::move(it->second);
			m_pendingScannerResults.erase(it);
		}
	}

	if (!results.empty()) {
		ScannerResult evt;
		evt.reqId = reqId;
		evt.items = std::move(results);
		pushEvent(Event{ evt });
	}
}
//! [scannerdataend]

//! [updateaccountvalue]
void IbkrClient::updateAccountValue(const std::string& key, const std::string& val,
	const std::string& currency, const std::string& accountName) {
	printf("UpdateAccountValue. Key: %s, Value: %s, Currency: %s, Account Name: %s\n",
		key.c_str(), val.c_str(), currency.c_str(), accountName.c_str());

	AccountValueUpdate update;
	update.key = key;
	update.value = val;
	update.currency = currency;
	update.accountName = accountName;

	AccountSummaryEvent event;
	event.accountValues[key] = update;
	pushEvent(Event{ event });
}
//! [updateaccountvalue]

//! [updateportfolio]
void IbkrClient::updatePortfolio(const Contract& contract, Decimal position,
	double marketPrice, double marketValue, double averageCost,
	double unrealizedPNL, double realizedPNL, const std::string& accountName) {
	printf("UpdatePortfolio. %s, %s @ %s: Position: %s, MarketPrice: %g, MarketValue: %g, AverageCost: %g, UnrealizedPNL: %g, RealizedPNL: %g, AccountName: %s\n",
		contract.symbol.c_str(), contract.secType.c_str(), contract.primaryExchange.c_str(),
		DecimalFunctions::decimalStringToDisplay(position).c_str(),
		marketPrice, marketValue, averageCost, unrealizedPNL, realizedPNL, accountName.c_str());

	PositionUpdate posUpdate;
	posUpdate.account = accountName;
	posUpdate.symbol = contract.symbol;
	posUpdate.secType = contract.secType;
	posUpdate.position = DecimalFunctions::decimalToDouble(position);
	posUpdate.marketPrice = marketPrice;
	posUpdate.marketValue = marketValue;
	posUpdate.averageCost = averageCost;
	posUpdate.unrealizedPNL = unrealizedPNL;
	posUpdate.realizedPNL = realizedPNL;

	AccountSummaryEvent event;
	event.positions.push_back(posUpdate);
	pushEvent(Event{ event });
}
//! [updateportfolio]

//! [position]
void IbkrClient::position(const std::string& account, const Contract& contract,
	Decimal position, double avgCost) {
	printf("Position. %s - Symbol: %s, SecType: %s, Currency: %s, Position: %s, Avg Cost: %g\n",
		account.c_str(), contract.symbol.c_str(), contract.secType.c_str(), contract.currency.c_str(),
		DecimalFunctions::decimalStringToDisplay(position).c_str(), avgCost);

	PositionUpdate posUpdate;
	posUpdate.account = account;
	posUpdate.symbol = contract.symbol;
	posUpdate.secType = contract.secType;
	posUpdate.position = DecimalFunctions::decimalToDouble(position);
	posUpdate.averageCost = avgCost;
	posUpdate.marketPrice = 0.0;
	posUpdate.marketValue = 0.0;
	posUpdate.unrealizedPNL = 0.0;
	posUpdate.realizedPNL = 0.0;

	AccountSummaryEvent event;
	event.positions.push_back(posUpdate);
	pushEvent(Event{ event });
}
//! [position]

#include "ibkr_c_api.h"
#include "ibkr.h"
#include "command.h"
#include "event.h"

#include <thread>
#include <chrono>
#include <cstring>
#include <queue>

/* Internal context: owns the IbkrClient and its worker thread */
struct IbkrCtx {
    IbkrClient* client;
    std::thread thread;
    std::queue<Event> pending;
};

static void copy_str(char* dst, size_t dst_size, const std::string& src) {
    if (!dst || dst_size == 0) return;
    std::strncpy(dst, src.c_str(), dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static std::queue<Event> take_events(IbkrCtx* ctx) {
    std::queue<Event> fresh = ctx->client->consumeEvents();
    if (ctx->pending.empty()) return fresh;

    std::queue<Event> merged;
    while (!ctx->pending.empty()) {
        merged.push(std::move(ctx->pending.front()));
        ctx->pending.pop();
    }
    while (!fresh.empty()) {
        merged.push(std::move(fresh.front()));
        fresh.pop();
    }
    return merged;
}

IbkrHandle ibkr_create(const char* host, int port, int clientId) {
    IbkrCtx* ctx = new IbkrCtx();
    ctx->client = new IbkrClient(host ? host : "127.0.0.1", port, clientId);
    ctx->thread = std::thread([ctx]() {
        ctx->client->processLoop();
    });
    return ctx;
}

void ibkr_destroy(IbkrHandle handle) {
    if (!handle) return;
    IbkrCtx* ctx = static_cast<IbkrCtx*>(handle);
    if (ctx->thread.joinable())
        ctx->thread.join();
    delete ctx->client;
    delete ctx;
}

void ibkr_connect(IbkrHandle /*handle*/) {
    /* processLoop() handles connection internally.
       Sleep to allow the handshake to complete. */
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void ibkr_disconnect(IbkrHandle handle) {
    if (!handle) return;
    IbkrCtx* ctx = static_cast<IbkrCtx*>(handle);
    DiscounnectCommand cmd;
    ctx->client->pushCommand(cmd);
}

void ibkr_start_scanner(IbkrHandle handle, int reqId,
                        const char* scanCode, double priceAbove) {
    if (!handle || !scanCode) return;
    IbkrCtx* ctx = static_cast<IbkrCtx*>(handle);
    StartScannerCommand cmd;
    cmd.reqId      = reqId;
    cmd.scanCode   = scanCode;
    cmd.locationCode = "STK.US";
    cmd.priceAbove = priceAbove;
    ctx->client->pushCommand(std::move(cmd));
}

int ibkr_poll_scanner(IbkrHandle handle,
                      CScannerItem* out_items, int max_items) {
    if (!handle || !out_items || max_items <= 0) return -1;
    IbkrCtx* ctx = static_cast<IbkrCtx*>(handle);

    std::queue<Event> events = take_events(ctx);
    int found_count = -1;
    while (!events.empty()) {
        Event evt = std::move(events.front());
        events.pop();

        if (found_count < 0) {
            if (auto* result = std::get_if<ScannerResult>(&evt.data)) {
                int count = 0;
                for (const auto& item : result->items) {
                    if (count >= max_items) break;
                    out_items[count].rank = item.rank;
                    copy_str(out_items[count].symbol, sizeof(out_items[count].symbol), item.symbol);
                    copy_str(out_items[count].secType, sizeof(out_items[count].secType), item.secType);
                    copy_str(out_items[count].currency, sizeof(out_items[count].currency), item.currency);
                    ++count;
                }
                found_count = count;
                continue;
            }
        }
        ctx->pending.push(std::move(evt));
    }
    return found_count;
}

void ibkr_request_account_data(IbkrHandle handle, const char* accountCode) {
    if (!handle) return;
    IbkrCtx* ctx = static_cast<IbkrCtx*>(handle);
    RequestAccountDataCommand cmd;
    if (accountCode) {
        cmd.accountCode = accountCode;
    }
    ctx->client->pushCommand(cmd);
}

int ibkr_poll_account_data(IbkrHandle handle,
                           CAccountValue* out_values, int max_values,
                           CPosition* out_positions, int max_positions,
                           int* out_value_count, int* out_position_count) {
    if (!handle || !out_values || !out_positions || !out_value_count || !out_position_count) return -1;
    if (max_values <= 0 || max_positions <= 0) return -1;

    *out_value_count = 0;
    *out_position_count = 0;

    IbkrCtx* ctx = static_cast<IbkrCtx*>(handle);
    std::queue<Event> events = take_events(ctx);

    while (!events.empty()) {
        Event evt = std::move(events.front());
        events.pop();

        if (auto* summary = std::get_if<AccountSummaryEvent>(&evt.data)) {
            for (const auto& kv : summary->accountValues) {
                if (*out_value_count >= max_values) break;
                const auto& val = kv.second;
                CAccountValue* out = &out_values[*out_value_count];
                copy_str(out->key, sizeof(out->key), val.key);
                copy_str(out->value, sizeof(out->value), val.value);
                copy_str(out->currency, sizeof(out->currency), val.currency);
                copy_str(out->accountName, sizeof(out->accountName), val.accountName);
                ++(*out_value_count);
            }

            for (const auto& pos : summary->positions) {
                if (*out_position_count >= max_positions) break;
                CPosition* out = &out_positions[*out_position_count];
                copy_str(out->account, sizeof(out->account), pos.account);
                copy_str(out->symbol, sizeof(out->symbol), pos.symbol);
                copy_str(out->secType, sizeof(out->secType), pos.secType);
                out->position = pos.position;
                out->marketPrice = pos.marketPrice;
                out->marketValue = pos.marketValue;
                out->averageCost = pos.averageCost;
                out->unrealizedPNL = pos.unrealizedPNL;
                out->realizedPNL = pos.realizedPNL;
                ++(*out_position_count);
            }
        } else {
            ctx->pending.push(std::move(evt));
        }
    }

    if (*out_value_count == 0 && *out_position_count == 0) return -1;
    return *out_value_count + *out_position_count;
}

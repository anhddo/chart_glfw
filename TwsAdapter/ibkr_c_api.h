#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle - C code never sees the C++ internals */
typedef void* IbkrHandle;

typedef struct {
    int   rank;
    char  symbol[32];
    char  secType[16];
    char  currency[8];
} CScannerItem;

typedef struct {
    char  key[64];
    char  value[32];
    char  currency[8];
    char  accountName[32];
} CAccountValue;

typedef struct {
    char  account[32];
    char  symbol[32];
    char  secType[16];
    double position;
    double marketPrice;
    double marketValue;
    double averageCost;
    double unrealizedPNL;
    double realizedPNL;
} CPosition;

/* Lifecycle */
IbkrHandle ibkr_create(const char* host, int port, int clientId);
void       ibkr_destroy(IbkrHandle handle);
void       ibkr_connect(IbkrHandle handle);
void       ibkr_disconnect(IbkrHandle handle);

/* Scanner */
void ibkr_start_scanner(IbkrHandle handle, int reqId,
                        const char* scanCode, double priceAbove);

/* Poll for scanner results (non-blocking).
   Returns number of items written into out_items (up to max_items).
   Returns -1 if no result is ready yet. */
int  ibkr_poll_scanner(IbkrHandle handle,
                       CScannerItem* out_items, int max_items);

/* Account data */
void ibkr_request_account_data(IbkrHandle handle, const char* accountCode);

/* Poll account/portfolio updates (non-blocking).
   Returns total number of updates written, or -1 if none are ready. */
int  ibkr_poll_account_data(IbkrHandle handle,
                            CAccountValue* out_values, int max_values,
                            CPosition* out_positions, int max_positions,
                            int* out_value_count, int* out_position_count);

#ifdef __cplusplus
}
#endif

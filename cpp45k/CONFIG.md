# Configuration Setup

## Quick Start

1. **Copy the template:**
   ```bash
   copy config.json.template config.json
   ```

2. **Edit `config.json`** with your details:
   ```json
   {
     "ibkr": {
       "account": "YOUR_ACCOUNT_HERE",  // Your IBKR account number
       "host": "127.0.0.1",              // TWS/Gateway host
       "port": 7497,                      // 7497=paper, 7496=live
       "clientId": 0                      // Client ID (0 is fine)
     },
     "scanner": {
       "defaultScanCode": "TOP_PERC_GAIN",
       "priceAbove": 5.0
     }
   }
   ```

3. **Important:** `config.json` is gitignored and will NOT be committed to the repository

## Configuration Options

### IBKR Settings

| Field | Description | Example |
|-------|-------------|---------|
| `account` | Your IBKR account number | `"U1234567"` or `"DU1234567"` |
| `host` | TWS/Gateway IP address | `"127.0.0.1"` |
| `port` | Connection port | `7497` (paper) or `7496` (live) |
| `clientId` | Client identifier | `0` (any unique number) |

### Scanner Settings

| Field | Description | Example |
|-------|-------------|---------|
| `defaultScanCode` | Initial scanner type | `"TOP_PERC_GAIN"` |
| `priceAbove` | Minimum stock price filter | `5.0` |

## Port Reference

- **7497** - TWS Paper Trading (demo account)
- **7496** - TWS Live Trading (real money)
- **4001** - IB Gateway Paper Trading
- **4000** - IB Gateway Live Trading

## Security Notes

✅ **DO:**
- Keep `config.json` private (it's in `.gitignore`)
- Use paper trading account for testing
- Verify TWS/Gateway connection settings

❌ **DON'T:**
- Commit `config.json` to Git
- Share your config file
- Hardcode credentials in source code

## Troubleshooting

**"Config file not found"**
- Copy `config.json.template` to `config.json`
- Make sure file is in the same directory as the executable

**"Account number not configured"**
- Edit `config.json` and replace `YOUR_ACCOUNT_NUMBER_HERE`
- Save the file and restart the application

**Connection fails**
- Verify TWS/Gateway is running
- Check port number matches TWS settings (API Settings)
- Enable "Socket Clients" in TWS API settings

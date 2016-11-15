
![image](https://cloud.githubusercontent.com/assets/5454938/20291702/13a59888-aab6-11e6-9d60-107941ea9492.png)
# HiredisProxy
#### Embedded Redis client for Redis




### What

A Redis module that allows forwarding write commands from slave(s) to master.

### Why

Scaling read-intensive Lua-based Redis app with horizontal scaling of slaves. Occasional writes are proxied to master.

### How

HiredisProxy is a simple [Redis module](https://github.com/RedisLabs/RedisModulesSDK) based on [Hiredis](https://github.com/redis/hiredis), a minimalistic C Redis client. It only runs on slaves and automatically connects to the master by pulling connection information from the current Redis state.

# Getting started

### How to use/test



step | slave | master
-----| ------------| -----------
Start two servers | `$ redis-server --port 6380  --dir . --loadmodule hiredisproxy.so`   |  `$ redis-server --port 6379`
Connect to both Redis servers | `$ redis-cli -p 6380` | `$ redis-cli -p 6379`
Monitor master to see what's happening | | `127.0.0.1:6379>  monitor`
Activate slave role | `127.0.0.1:6380>  slaveof 127.0.0.1 6379` |  `1479175823.971904 [0 127.0.0.1:62543] "PING"`
Proxy a command to master | `127.0.0.1:6380> hiredis.proxy set more lemmings` | `1479175834.943794 [0 127.0.0.1:62544] "set" "more" "lemmings"`
Access it locally (from replication) | `127.0.0.1:6380> get more` | 
...  | `lemmings` |

### Build/Install

- Clone the redis modules sdk
```
git clone https://github.com/RedisLabs/RedisModulesSDK.git
```
- Clone HiredisProxy into it
```
cd RedisModulesSDK
git clone https://github.com/synku/HiredisProxy
```
- Make
```
cd HiredisProxy
make
```

### Notes

- This is not yet tested enough to be considered production ready, please help!
- Everything is synchronous so the proxying is costly timing-wise
- How else would you use this?

This project was born from our ever-growing scaling efforts at [ClarityAd] (https://www.clarityad.com).
We're hiring! Hit me up at [@j_rom_](https://twitter.com/j_rom_)

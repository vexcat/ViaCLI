# ViaCLI
My VEX Via CLI written in C++!

It downloads SQL-dumps from dwabtech's servers just like their iOS & Android apps do, then dumps the resulting data in JSON form.
```
./ViaCLI list-events dump
```
Note that this would take a long time the first time it is ran, because there are a lot of VEX events and an *even bigger* amount of metadata about them. Only the event list is cached to a file (evt_list.db), getting data on a particular event uses an in-memory database.

You can also perform an SQL query on the database that was fetched, by using the "do" parameter.
```
./ViaCLI list-events do "SELECT events.sku,name FROM events JOIN events_have_teams ON events_have_teams.sku = events.sku WHERE teamnum = '8301E'"
Downloading SQL data...
https://data.vexvia.dwabtech.com/api/v3/events?since=618950&schema=206&timeout=0
Got 0 lines of SQL data.
Executing SQL script...
[{"name":"2018 Sweetwater High School Robotics League","sku":"re-vrc-18-5109"},{"name":"2018 South San Diego Regional VRC Tournament","sku":"re-vrc-18-5206"},{"name":"9th Annual Chula Vista VEX Regional","sku":"re-vrc-18-5348"},{"name":"2019 VEX Robotics World Championship - VEX Robotics Competition High School Division","sku":"re-vrc-18-6082"},{"name":"Signature Event: Google VEX Turning Point High School Tournament","sku":"re-vrc-18-6359"},{"name":"2018 San Diego December Regional VEX Robotics Tournament","sku":"re-vrc-18-7278"},{"name":"2019 California VEX VRC High School State Championship - SAN DIEGO","sku":"re-vrc-18-7675"}]
Database named evt_list.db successfully closed.
```
This filters the event list for only events with 8301E attending. (All output besides the JSON data is going to stderr).

You can do the same for a particular event's data:
```
./ViaCLI detail-event re-vrc-18-5109 do "SELECT name FROM awards WHERE teamnum = '8301E'"
Downloading SQL data...
https://data.vexvia.dwabtech.com/api/v3/event/re-vrc-18-5109?since=0&schema=0&timeout=0
Got 1298 lines of SQL data.
Executing SQL script...
Downloading SQL data...
https://data.vexvia.dwabtech.com/api/v3/event/re-vrc-18-5109?since=379005&schema=206&timeout=0
Got 0 lines of SQL data.
Executing SQL script...
[{"name":"Tournament Champion"},{"name":"Robot Skills Champion"},{"name":"Think Award"}]
Database named  successfully closed.
```
That's all the awards we won at our league championships last year.

Lastly, you can use the `--long-poll` option to keep waiting for new changes to VEX Via. Changes will be sent to stdout as lines of JSON patches to the original output. Note that this has not been tested yet -- if anyone has a competition coming up and wants to test this that would be awesome!

Hope you do some cool data analysis with this tool -ungato/vexcat

# Compiling
On Linux (Ubuntu 18.04), you'll need to first [get the latest CMake, the one in your distro's repo is probably too outdated](https://askubuntu.com/a/595441/), then do this in your shell:
```
sudo apt install build-essential libsqlite3-dev libcurlpp-dev libcurl4-openssl-dev
cmake .
make
```
You should now be able to execute `./ViaCLI`! Have fun with it.

# Shared-world clock trial

This trial tests the time rule proposed in
[A living world at human scale](https://github.com/atimics/crownlesscarriage/pull/915). One clock owns
the elapsed seconds for every active company. A fast advance starts when every
company has booked a timed activity. It ends at the earliest completed activity.
The other bookings keep their remaining time.

The focused test books a 48-hour trip for one company. The second company works
for six hours, explores for 30 seconds at normal speed, and then alternates
rest and work. The trip completes at world second 172,800. Both companies reach
their final decision at that same second. Completion IDs arrive in stable order.

Normal ticks also move active journeys. A tick stops at the next completion, so
the world can ask for a new decision before more time passes. Rejected bookings
and advances leave the clock unchanged.

This is a simulation-level policy trial. The next play trial will connect the
clock to actual company activities, saved journeys, the server, and a ready
screen. It will measure agreement wait and the real time needed for a two-day
trip with two players. That evidence will settle the shared-session rules
before the regional distance layout is committed.

Run the focused check with:

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless --target shared_clock_tests
ctest --test-dir out/build/headless -R coordinated_company_clock --output-on-failure
```


## 1. Why must a variable shared between the ISR and a task be `volatile`?

A variable shared between an ISR and a normal task can change asynchronously. If the variable is not declared `volatile`, the compiler is allowed to assume that its value does not change unexpectedly.

For example:

```c
while (!flag)
{
}
```

Without `volatile`, the compiler may read `flag` once and keep the value in a register instead of reading it again on every loop iteration. The compiler does not know that an ISR can change `flag` asynchronously.

Therefore, even if the ISR writes:

```c
flag = true;
```

the task may continue using the old cached value and the loop may never finish.

Declaring the variable as:

```c
volatile bool flag;
```

tells the compiler that the value can change outside the normal flow of the current code, so every access must actually read or write the variable.

In this implementation, the button event is passed through a FreeRTOS queue instead of using a shared flag. The queue operations are designed to safely transfer data between the ISR and the task.

---

## 2. Why are `ESP_LOGI()` and `vTaskDelay()` dangerous inside the ISR?

An ISR runs in interrupt context, not normal task context.

`ESP_LOGI()` can perform operations that are not appropriate for interrupt context, including accessing locks, buffers, and other code that may not be safe to execute from an ISR. Depending on the implementation, it can also take a significant amount of time.

`vTaskDelay()` is even more clearly inappropriate because it is a FreeRTOS task-scheduling operation. An ISR cannot block or put itself to sleep like a normal task.

The important mechanism is that an ISR must finish quickly and must not block. If an ISR blocks or performs operations that depend on normal task scheduling, it can cause interrupt latency, watchdog problems, deadlocks, or system crashes.

Therefore, the ISR in this program only captures the GPIO state and timestamp and sends an event to the FreeRTOS queue using:

```c
xQueueSendFromISR()
```

All logging, timing decisions, counter arithmetic, and display updates are performed by the normal task.

---

## 3. Comparison between polling and interrupt-driven implementations

The interrupt-driven implementation is more responsive because the CPU reacts to the GPIO edge immediately instead of waiting until the polling loop checks the button again.

In the polling implementation, the worst-case detection latency is approximately one polling interval. For example, if the button is checked every 10 ms, an edge may have to wait almost 10 ms before it is detected.

With GPIO interrupts, the hardware generates an interrupt as soon as the edge occurs, so the detection latency is normally much smaller, although there is still a small interrupt latency caused by the CPU and interrupt system.

The interrupt-driven implementation also uses less CPU while the button is untouched. With polling, the CPU repeatedly wakes up and checks the GPIO pin even when nothing has happened. With interrupts, the CPU does not need to continuously check the button.

However, the interrupt-driven implementation was more difficult to get right. Polling made the gesture state machine relatively straightforward because the program continuously knew the current button state. With interrupts, the program receives discrete events, so debounce, double-click timing, and long-press repetition must be handled carefully.

The long-press repeat is especially important. The ISR only reports the press and release edges. While the button remains held, there are no new edges, so the gesture task must use a timeout to generate the 500 ms repeat events.

Therefore:

| Feature                      | Polling                          | Interrupt                              |
| ---------------------------- | -------------------------------- | -------------------------------------- |
| Button detection             | Repeatedly checks GPIO           | Hardware reports edge                  |
| CPU usage when idle          | Higher                           | Lower                                  |
| Worst-case detection latency | About one polling interval       | Much smaller interrupt latency         |
| Gesture implementation       | Easier                           | More complex                           |
| Long-press repeat            | Easy with periodic polling       | Requires task timeout                  |
| Bounce handling              | Can sometimes be less noticeable | Every bounce can generate an interrupt |
| Overall efficiency           | Lower                            | Higher                                 |
| Difficulty                   | Easier                           | Harder                                 |

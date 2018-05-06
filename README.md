# OperatingSystems-_XV6_Signals_and_Synchronization


The assignment main goal is to teach you about XV6 synchronization mechanisms and process management.
First, you will learn and use the Compare And Swap (CAS) instruction by adding support for its atomic
use. Next, you will add simple signals for XV6 that will also use CAS. You will use signals to create an
application that coordinates multiple processes. In the last part of the assignment you will replace XV6s
process management synchronization mechanism with your own lock-free implementation (using CAS). This
will establish an environment that supports a multi-processor/core.
The assignment is composed of four main parts:
1. Implement a simple signals framework.
2. Using the signal framework, create a multi process application.
3. Add CAS support for XV6.
4. Enhance the XV6 process management mechanism via a lock-free synchronization algorithm.

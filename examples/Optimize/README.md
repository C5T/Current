## Optimize

### Intro

This directory contains the example of using the `FnCAS` library to solve an optimization problem.

The problem considered is the two-player zero-sum matrix game: For an `N` by `N` matrix `A` where `A[i][j]` is the payoff from player two to player one as players have picked up strategies `i` and `j` respectively, find the optimal mixed strategies for both players. The mixed strategy for each player is a simplex of dimension `N` where each element is the probability of the player picking this strategy, which corresponds to the row or column of the matrix for players one and two respectively.

### Reasoning

The zero-sum matrix game is a special case of linear programming problem. It is a good fit for this example, as:

* The solution is computationally easy to verify, but computationally expensive to find.
* For all but very narrow cases, the solution is unique.
* There exists a simple iterative algorithm, which, albeit slow, converges to the optimal solution.
* Random matrices do well as regression test inputs.

### Mathematics

It is the Nash equilibrium that the solution follows the minimax principle: The most player one can win from player two is equal to the least player two can lose to player one, and vice versa.

The example in this directory implements two solutions:

* `smoke`: The slow, iterative one.
* `optimize`: The more effective, gradient-descent-based one.

## Implementations

### Smoke Test

A smoke-test solution (`smoke.cc`) is the Brown-Robinson iterative method:
* Each player has a "bag of strategies" to play, and begins with this bag containing one occurrence of each strategy. This player's mixed strategy is assumed to be the weighted sum of all the strategies in their bag.
* Players take turns at optimizing their bags.
* At each optimization half-step per-player, knowing the opponent's mixed strategy -- the contents of their bag -- the player under consideration picks their strategy that best counters the present state of the opponent's bag, and adds this newly picked strategy into their own bag.
* For this exercise, to construct the solution we look at the strategies added into players' bags in the past half of the iterative process. This improves convergence slightly, as early iterations often pick the strategies that don't even present in the final solution, and, while the process would converge nonetheless, it would take longer to outweigh them than to disregard them.

### Optimization-Based Solution

The optimization-based solution (`optimize.cc`) solves the problem by converting it into an optimization problem. To accomplish this:

* The very linear programming constraints are used as constraints:
  * Player one's payoff from each pure strategy is less or equal than the equilibrium payoff.
  * Player two's payoff from each pure strategy is greater or equal than the equilibrium payoff.
* Two tricks are used to make the problem convex:
  * Each player's strategy is a simplex that's just `s[i] = sqr(x[i]) / sum(sqr(x[i]))`. (I've tried other. -- D.K.)
  * The penalty is the "softmax penalty" function, which is `sqr(v + sqrt(v * v))`. (I've tried other. -- D.K.)

As this example is only a regression test for the larger-scale optimizer, I didn't bother do more than just the above. -- D.K.

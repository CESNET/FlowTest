# FlowTest Profile Sampler

Due to the size of profiles created in the 100Gb environments, it is necessary to perform some sort of sampling.
However, sampling biflows in the profile randomly is likely to change some important traffic characteristics.
Namely:
* packets to bytes ratio
* biflows to packets ratio
* biflows to bytes ratio
* IPv6 to IPv4 ratio
* proportional representation of L4 protocols in the profile (top 5)
* proportional representation of source and destination ports in the profile (top 10)
* proportional representation of average packet sizes in biflows

The profile sampling is implemented using a genetic algorithm to find a sample
(further referred to as *solution*) of the profile in which none of the specified characteristics
(further referred to as *metrics*) deviate more than a certain percentage from the original profile.

## How It Works

Each individual in the population is encoded as a bitmap where each index indicates
whether the biflow in the profile with such index is present in the solution or not.

At the beginning of the genetic algorithm, several individuals are initialized randomly.
This is done by putting random number of ones (with respect to the provided lower and upper bounds)
into the bitmap of each individual.

The genetic algorithm runs for configurable number of generations or until acceptable solution is discovered.
Each generation consists of the following stages:
* **Selection** - select parents from the current population for mating
* **Crossover** - parents mate producing the same amount of offsprings
* **Mutation** - all offsprings mutate some of its genes
* **Repair** - add resp. remove some of the offspring genes if they have too few resp. too many genes
* **Fitness** - compute fitness value for every offspring
* check if any of the offsprings can be accepted as a solution to our problem
* offsprings become the parents in the next generation

**Fitness** value of an individual is set to **100 minus percentual deviation of every metric from the original metrics**.
Solutions which have superb results in some metrics and terrible in other metrics are considered worse
than solutions where all metric deviations are mediocre.
To further penalize solutions where some metrics deviate more than 1% from the original,
all metric deviations are squared.
Generally, it is expected that deviations start to get below 1% for solutions with fitness above 94.

**Selection** of individuals for the next generation is done using
[stochastic universal sampling](https://en.wikipedia.org/wiki/Stochastic_universal_sampling)
which increases the chance for individuals with worse fitness value to survive the selection process.
This reduces evolution pressure and decreases the chance of the algorithm to get stuck in a local optimum.

**Crossover** is done from 4 individuals using standard
[two-points strategy](https://en.wikipedia.org/wiki/Crossover_(genetic_algorithm)).

**Mutation** operator exhibits the most influence on the outcome of the genetic algorithm.
It shuffles random part of the genome of the solution. Shuffling is perfect in this case as it does
not introduce new biflows into the solution, thus the number of biflows stays within the sampling bounds.
The mutation pressure is determined by the average fitness value in the population.
If the average fitness value is < 80, more genes will be mutated in an attempt to find more diverse solutions.
If the average fitness value is >= 80, mutate fewer genes to incrementally improve solutions in the population.

The algorithm runs for a fixed number of generations or until it finds a solution
which satisfies the criteria for acceptance.

### Usage

Start the profile sampler by running `ft-profile-sampler <args>`.

Arguments:
```
  -h, --help            show this help message and exit
  -u VALUE, --max-sampling VALUE
                        maximum sampling value (mandatory, must be between 0 and 1)
  -l VALUE, --min-sampling VALUE
                        minimum sampling value (mandatory, must be between 0 and 1, lower than maximum sampling value)
  -i FILE, --input FILE
                        path to a CSV file containing the input profile (mandatory)
  -o FILE, --output FILE
                        path to a file where the sample should be written (mandatory)
  -m FILE, --metrics FILE
                        path to a file where metrics of the result should be (mandatory)
  -d VALUE, --deviation VALUE
                        acceptable deviation of each key metric from the original profile metric
                        (must be between 0 and 1, default: 0.005)
  -s VALUE, --seed VALUE
                        seed for the random number generator to reproduce specific run
  -g VALUE, --generations VALUE
                        number of generations (default: 500)
  -p VALUE, --population VALUE
                        population size (default: 16)
  -q, --quiet           do not print any runtime information
  -t, --port-limit VALUE
                        Omit ports which proportional representation in the profile is less than
                        a threshold when calculating fitness (must be between 0 and 1, default: 0.005).
  -r, --proto-limit VALUE
                        Omit protocols which proportional representation in the profile is less than
                        a threshold when calculating fitness (default: 0.005).
```

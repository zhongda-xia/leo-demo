`Leo.py` is a python library that deals with LEO constellations to output data for visualization (via Cesium by AGI) and NDN simulation (via ndnSIM 2.x).

`demo.py` uses `Leo.py` to generate sample data for visualization and simulation.

`viz/` directory provides a visualization environment to see a LEO constellation network in action.

`sim/` directory contains the ndnSIM code, including an adapted ndnSIM module, and a stand-alone ndnSIM scenario. Obtain vanilla ns-3 from https://github.com/named-data-ndnSIM/ns-3-dev, commit ID: `cc8cccf`.

# Python dependencies

* Python version: 3.x (including for running the waf build tools)

* Python libraries: `pip install ephem skyfield czml networkx tqdm`

# Guidelines for running the demo

## Generate sample output

Run `demo.py`, should take a few minutes.

`demo.py` simulates a Starlink-like constellation, and generates the following outputs:
- a CZML file (czml_files/starlink.czml) for visualization, including the routes between Beijing (consumer) and Chicago (producer).
- input files for ndnSIM to simulate the traffic between Beijing (consumer) and Chicago (producer).

## Run visualization

Enter the `viz/` directory and build the visualization environment with:

`npm install`

Then start the local server with:

`npm start`

Visit `localhost:8080` to see the visualization demo.

Click the "starlink" button on the top left to load the constellation, should take a few seconds in a local environment. Remote access should also work as the server listens on 0.0.0.0.

## Run simulation

First prepare the environment as instructed by ndnSIM site.
Then obtain ns-3 and checkout commit `cc8cccf`, and link `sim/ndnSIM` to `src/ndnSIM`. 
Finally build ns-3 and install to the system (ns-3 must be installed for the stand-alone scenario to work):

`./waf configure --disable-python -d debug`

`./waf`

`sudo ./waf install`

Enter `sim/scenario` and build the code:

`./waf configure --debug`

`./waf`

Run the simulation by invoking `run.py`:

`./run.py -s -g leo-consumer`

`run.py` invokes multiple simulations with different consumer rates and Interest retransmission mechanisms. 
Trace files for these runs are stored in a new directory under the `results/` directory, which is named after the invocation time.
Each simulation run generates an L3 traffic trace (`ndn::sat::L3RateTracer`), and an data retrieval delay trace (`ndn::sat::AppDelayTracer`)
The name of each trace file contains the list of parameters and their values, followed by the type of trace.

# Comments on the code

TODO
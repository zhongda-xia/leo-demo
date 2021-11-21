`Leo.py` is a python library (use Python 3.x) that deals with LEO constellations to output data for visualization (via Cesium by AGI) and NDN simulation (via ndnSIM 2.x).

`viz/` directory provides a visualization environment to see a LEO constellation network in action.

`sim/` directory contains the ndnSIM code, including an adapted ndnSIM module, and a stand-alone ndnSIM scenario. Obtain ns3 from https://github.com/named-data-ndnSIM/ns-3-dev, commit ID: cc8cccf.

# Dependencies

`pip install ephem skyfield czml networkx tqdm`

# Running the demo

First, run `demo.py` with Python 3.x:

`python demo.py`

`demo.py` simulates a Starlink-like constellation, and generates outputs for external uses:
- generate a CZML file for visualization, which includes visualization of the routes between Beijing (consumer) and Sau Paulo (producer).
- generate input files for ndnSIM to simulate the traffic between Beijing (consumer) and Sau Paulo (producer).

After running `demo.py` (which should take several minutes), enter the `viz/` directory and build the visualization environment with:

`npm install`

Then start the local server with:

`npm start`

Visit `localhost:8080` to see the visualization demo.

Click the "starlink" button on the top left to load the constellation. Loading could take a while, progress bar not yet implemented.
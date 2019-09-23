var app = new Vue({
    el: '#app',
    data: {
	local_info: null,
	local_config: null,
	ws_state: "Initialising",
    },
    filters: {
    },
    mounted () {
	axios
	    .get('/api/local.json')
	    .then(response => {
		this.local_info = response.data
	    })
	    .catch(error => {
		console.log(error)
	    });
	axios
	    .get('/api/local-config.json')
	    .then(response => {
		this.local_config = response.data
	    })
	    .catch(error => {
		console.log(error)
	    });
    }
});

var cytoscape_main = cytoscape({
    container: document.getElementById('cytoscape_main'),
    elements: [
	{ // node n1
	    data: { // element data (put json serialisable dev data here)
		id: 'root' // mandatory (string) id for each element, assigned automatically on undefined
		// (`parent` can be effectively changed by `eles.move()`)
	    },
	},
    ],

    layout: {
	name: 'grid',
	rows: 1
    },

    // so we can see the ids
    style: [
	{
	    selector: 'node',
	    style: {
		'label': 'data(id)',
		'text-valign': 'top',
		'text-halign': 'left'
	    }
	}
    ]
});

var dagre_options = {
    // dagre algo options, uses default value on undefined
    nodeSep: undefined, // the separation between adjacent nodes in the same rank
    edgeSep: undefined, // the separation between adjacent edges in the same rank
    rankSep: undefined, // the separation between each rank in the layout
    rankDir: undefined, // 'TB' for top to bottom flow, 'LR' for left to right,
    ranker: undefined, // Type of algorithm to assign a rank to each node in the input graph. Possible values: 'network-simplex', 'tight-tree' or 'longest-path'
    minLen: function( edge ){ return 1; }, // number of ranks to keep between the source and target of the edge
    edgeWeight: function( edge ){ return 1; }, // higher weight edges are generally made shorter and straighter than lower weight edges

    // general layout options
    fit: true, // whether to fit to viewport
    padding: 30, // fit padding
    spacingFactor: undefined, // Applies a multiplicative factor (>0) to expand or compress the overall area that the nodes take up
    nodeDimensionsIncludeLabels: false, // whether labels should be included in determining the space used by a node
    animate: true, // whether to transition the node positions
    animateFilter: function( node, i ){ return true; }, // whether to animate specific nodes when animation is on; non-animated nodes immediately go to their final positions
    animationDuration: 500, // duration of animation in ms if enabled
    animationEasing: undefined, // easing of animation if enabled
    boundingBox: undefined, // constrain layout bounds; { x1, y1, x2, y2 } or { x1, y1, w, h }
    transform: function( node, pos ){ return pos; }, // a function that applies a transform to the final node position
    ready: function(){}, // on layoutready
    stop: function(){} // on layoutstop
};

class CNFTree {
    constructor(cy) {
	this.cy = cy;
	this.socket = null;
	this.url = "ws://" + window.location.href.substring(7);

	this.connect(this.url);
    }

    connect(url) {
	app.ws_state = "Connecting...";
	console.log("Using URL for websocket: " + this.url);
	this.socket = new WebSocket(url);
	this.socket.onopen = this.onWSOpen;
	this.socket.onmessage = this.onWSMessage;
    }

    onWSOpen() {
	app.ws_state = "Connected!";
    }

    onWSMessage() {
	app.ws_state = "Message!";
    }
}

var cnfTree = new CNFTree(cytoscape_main);

cytoscape_main.layout({ name: 'dagre', options: dagre_options }).run();

cytoscape_main.add({
    data: {id: "n1"}
});

cytoscape_main.add({
    data: {source: "root", target: "n1"}
});

cytoscape_main.layout({ name: 'dagre', options: dagre_options }).run()

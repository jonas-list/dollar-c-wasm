var statusText = document.getElementById("status").innerHTML;
var canvas = document.getElementById("canvas");
var context = canvas.getContext("2d");

var position = { x: 0, y: 0};
var lastPoint = { x: 0, y: 0};
var minDistance = 10;
var drawing = false;
var values = [];
var numPoints = 0;

canvas.addEventListener("mousemove", draw);
canvas.addEventListener("mousedown", setPosition);
canvas.addEventListener("mouseenter", setPosition);
canvas.addEventListener("mouseup", submit);
document.addEventListener("mouseup", lift);

var Module = 
{
	onRuntimeInitialized: function() 
	{
		Module._load_templates();
	}
};

function setPosition(e)
{
	var rect = canvas.getBoundingClientRect();
    position.x = e.clientX - rect.left;
    position.y = e.clientY - rect.top;
}

function distance(p1, p2)
{
	return Math.sqrt((p1.x - p2.x) ** 2 + (p1.y - p2.y) ** 2);
}

function draw(e)
{
    if(e.buttons !== 1) return;

    if (!drawing)
    {
        context.clearRect(0, 0, canvas.width, canvas.height);
        drawing = true;
    }
	
	if(distance(lastPoint, position) > minDistance)
	{
    	values.push(position.x);
    	values.push(position.y);
		numPoints++;

		lastPoint.x = position.x;
		lastPoint.y = position.y;
	}

    context.beginPath();

    context.lineWidth = 3;
    context.lineCap = "round";
    context.strokeStyle = "black";

    context.moveTo(position.x, position.y);
    setPosition(e);
    context.lineTo(position.x, position.y);

    context.stroke();
}

function submit(e)
{
    drawing = false;

	const ptr = arrayToHeap(values);	
	Module._construct_stroke(ptr, numPoints);

	// console.log(Module._get(3));	

	Module._recognize();
	Module._free(ptr);
	
    values = [];
	numPoints = 0;
}

function lift(e)
{
    drawing = false;
}

function arrayToHeap(array)
{
	const f64Array = new Float64Array(array);
	const ptr = Module._malloc(f64Array.length * f64Array.BYTES_PER_ELEMENT);
	Module.HEAPF64.set(f64Array, ptr / f64Array.BYTES_PER_ELEMENT);
	
	return ptr;
}	

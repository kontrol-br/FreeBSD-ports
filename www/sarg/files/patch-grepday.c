--- grepday.c.orig	2020-01-16 10:35:42 UTC
+++ grepday.c
@@ -43,6 +43,10 @@ struct GraphDataStruct
 	int gray;
 	int silver;
 	int black;
+	//! The color of the unobtrusive plot grid.
+	int grid;
+	//! A modern, colorblind-friendly palette used to distinguish days.
+	int palette[7];
 	//! The color of the top side of a graph bar.
 	int color1;
 	//! The color of the right side of a graph bar.
@@ -242,48 +246,24 @@ static void Sarg_gdImageStringFT (struct GraphDataStru
 	}
 }
 
-static void bar(struct GraphDataStruct *gdata,int x1,double y,const char *label)
+static void bar(struct GraphDataStruct *gdata,int x1,double y,const char *label,int index)
 {
-	gdPoint points[4];
-	int val=0;
+	int val;
 	int width2;
+	int fill;
 
-	val=gdata->BottomGraph+5-(int)(y*gdata->YScale+0.5);
+	val=gdata->BottomGraph-(int)(y*gdata->YScale+0.5);
 	width2=(int)(gdata->XScale/4.);
+	if (width2<3) width2=3;
+	fill=gdata->palette[index%7];
 
-	// front side of the bar
-	if (val<gdata->BottomDepth)
-		gdImageFilledRectangle(gdata->im, x1-width2, val, x1+width2, gdata->BottomDepth, gdata->color3);
-
-	// top side of the bar
-	points[0].x = x1-width2+5;
-	points[0].y = val-5;
-	points[1].x = x1-width2;
-	points[1].y = val;
-	points[2].x = x1+width2;
-	points[2].y = val;
-	points[3].x = x1+width2+5;
-	points[3].y = val-5;
-	gdImageFilledPolygon(gdata->im, points, 4, gdata->color1);
-
-	gdImageLine(gdata->im, x1+2, val-2, x1+2, val-10, gdata->dimgray);
-	gdImageFilledRectangle(gdata->im, x1-8, val-20, x1+12, val-10, gdata->goldenrod);
-	gdImageRectangle(gdata->im, x1-8, val-20, x1+12, val-10, gdata->goldenrod2);
-
-	Sarg_gdImageStringFT(gdata,gdata->black,GraphFont,6,0.0,x1+2,val-12,label,TRP_BottomCenter);
-
-	// lateral side of the bar
-	if (val<gdata->BottomDepth) {
-		points[0].x = x1+width2+5;
-		points[0].y = val-5;
-		points[1].x = x1+width2;
-		points[1].y = val;
-		points[2].x = x1+width2;
-		points[2].y = gdata->BottomDepth;
-		points[3].x = x1+width2+5;
-		points[3].y = gdata->BottomGraph;
-		gdImageFilledPolygon(gdata->im, points, 4, gdata->color2);
+	// Flat columns are easier to compare than the former simulated 3D bars.
+	if (val<gdata->BottomGraph) {
+		gdImageFilledRectangle(gdata->im,x1-width2,val,x1+width2,gdata->BottomGraph,fill);
+		gdImageLine(gdata->im,x1-width2,val,x1+width2,val,gdata->black);
 	}
+
+	Sarg_gdImageStringFT(gdata,gdata->black,GraphFont,7,0.0,x1,val-5,label,TRP_BottomCenter);
 
 	return;
 }
@@ -380,7 +360,7 @@ static void greport_draw_yaxis(const struct PlotStruct
 	for (y=y0-gdata->YTickSpace ; y>=gdata->TopGraph ; y-=gdata->YTickSpace) {
 		gdImageLine(gdata->im, xtick, y+yt, xexterior, y+yt, gdata->dimgray);
 		gdImageLine(gdata->im, xexterior, y+yt, xinterior, y, gdata->dimgray);
-		gdImageLine(gdata->im, xinterior, y, gdata->RightGraph, y, gdata->dimgray);
+		gdImageLine(gdata->im, xinterior, y, gdata->RightGraph, y, gdata->grid);
 		switch (pdata->ytype)
 		{
 			case PTG_LinBin:
@@ -420,15 +400,13 @@ static void greport_plot(const struct userinfostruct *
 	time_t t;
 	struct tm *local;
-	gdPoint points[4];
 	struct GraphDataStruct gdata;
-	const int ImgXSize=720;
-	const int ImgYSize=480;
-	const int LeftMargin=60;
-	const int RightMargin=20;
-	const int TopMargin=60;
-	const int BottomMargin=60;
+	const int ImgXSize=920;
+	const int ImgYSize=430;
+	const int LeftMargin=72;
+	const int RightMargin=28;
+	const int TopMargin=72;
+	const int BottomMargin=58;
 	const int TickLength=3;
-	const int ZTickLength=5;
 	double yval;
 	char blabel[50];
 	double logpmin;
@@ -442,20 +420,28 @@ static void greport_plot(const struct userinfostruct *
 	gdata.LeftGraph=LeftMargin;
 	gdata.RightGraph=ImgXSize-RightMargin;
 	gdata.TopGraph=TopMargin;
-	gdata.BottomDepth=gdata.BottomGraph+5;
+	gdata.BottomDepth=gdata.BottomGraph;
 	gdata.XScale=(double)(gdata.RightGraph-gdata.LeftGraph)/(pdata->npoints+1);
 	if (greport_compute_yaxis(pdata,&gdata)<0) return;
 	gdata.TickLength=TickLength;
 
 	// first allocated color is the background
-	gdata.lavender = gdImageColorAllocate(gdata.im, 230, 230, 250);
-	gdata.gray = gdImageColorAllocate(gdata.im, 192, 192, 192);
-	gdata.silver = gdImageColorAllocate(gdata.im, 211, 211, 211);
-	gdata.black = gdImageColorAllocate(gdata.im, 0, 0, 0);
-	gdata.dimgray = gdImageColorAllocate(gdata.im, 105, 105, 105);
-	gdata.darkblue = gdImageColorAllocate(gdata.im, 0, 0, 139);
-	gdata.goldenrod = gdImageColorAllocate(gdata.im, 234, 234, 174);
-	gdata.goldenrod2 = gdImageColorAllocate(gdata.im, 207, 181, 59);
+	gdata.lavender = gdImageColorAllocate(gdata.im,255,255,255);
+	gdata.gray = gdImageColorAllocate(gdata.im,203,213,225);
+	gdata.silver = gdImageColorAllocate(gdata.im,248,250,252);
+	gdata.black = gdImageColorAllocate(gdata.im,15,23,42);
+	gdata.dimgray = gdImageColorAllocate(gdata.im,100,116,139);
+	gdata.darkblue = gdImageColorAllocate(gdata.im,29,78,216);
+	gdata.goldenrod = gdImageColorAllocate(gdata.im,245,158,11);
+	gdata.goldenrod2 = gdImageColorAllocate(gdata.im,180,83,9);
+	gdata.grid = gdImageColorAllocate(gdata.im,226,232,240);
+	gdata.palette[0] = gdImageColorAllocate(gdata.im,37,99,235);
+	gdata.palette[1] = gdImageColorAllocate(gdata.im,13,148,136);
+	gdata.palette[2] = gdImageColorAllocate(gdata.im,245,158,11);
+	gdata.palette[3] = gdImageColorAllocate(gdata.im,249,115,22);
+	gdata.palette[4] = gdImageColorAllocate(gdata.im,139,92,246);
+	gdata.palette[5] = gdImageColorAllocate(gdata.im,225,29,72);
+	gdata.palette[6] = gdImageColorAllocate(gdata.im,8,145,178);
 
 	if (strcmp(GraphDaysBytesBarColor,"orange") == 0) {
 		gdata.color1 = gdImageColorAllocate(gdata.im, 255, 233, 142);
@@ -491,46 +477,10 @@ static void greport_plot(const struct userinfostruct *
 	}
 
-	// rectangle around the image
-	gdImageRectangle(gdata.im, 0, 0, ImgXSize-1, ImgYSize-1, gdata.dimgray);
-	// backtround of the graph
+	// Flat plot surface and quiet axes replace the former simulated 3D frame.
 	gdImageFilledRectangle(gdata.im, LeftMargin, gdata.TopGraph, gdata.RightGraph, gdata.BottomGraph, gdata.silver);
-
-	// depth of the left Y axis
-	points[0].x = gdata.LeftGraph-10;
-	points[0].y = gdata.TopGraph+5;
-	points[1].x = gdata.LeftGraph-10;
-	points[1].y = gdata.BottomDepth;
-	points[2].x = gdata.LeftGraph;
-	points[2].y = gdata.BottomGraph;
-	points[3].x = gdata.LeftGraph;
-	points[3].y = gdata.TopGraph;
-	gdImageFilledPolygon(gdata.im, points, 4, gdata.gray);
-
-	// depth of the bottom X axis
-	points[0].x = gdata.LeftGraph;
-	points[0].y = gdata.BottomGraph;
-	points[1].x = gdata.LeftGraph-10;
-	points[1].y = gdata.BottomDepth;
-	points[2].x = gdata.RightGraph-10;
-	points[2].y = gdata.BottomDepth;
-	points[3].x = gdata.RightGraph;
-	points[3].y = gdata.BottomGraph;
-	gdImageFilledPolygon(gdata.im, points, 4, gdata.gray);
-
-	// vertical exterior line of the depth
-	gdImageLine(gdata.im, LeftMargin-10, TopMargin+5, LeftMargin-10, gdata.BottomDepth+ZTickLength, gdata.black);
-	// horizontal exterior line of the depth
-	gdImageLine(gdata.im, LeftMargin-10-ZTickLength, gdata.BottomDepth, gdata.RightGraph-10, gdata.BottomDepth, gdata.black);
-	// diagonal line between the two depths
-	gdImageLine(gdata.im, LeftMargin-10, gdata.BottomDepth, LeftMargin, gdata.BottomGraph, gdata.black);
-	// vertical left line of the graph
-	gdImageLine(gdata.im, LeftMargin, gdata.BottomGraph, LeftMargin, gdata.TopGraph, gdata.black);
-	// horizontal bottom line of the graph
-	gdImageLine(gdata.im, LeftMargin, gdata.BottomGraph, gdata.RightGraph, gdata.BottomGraph, gdata.black);
-	// vertical right line of the graph
-	gdImageLine(gdata.im, gdata.RightGraph, gdata.TopGraph, gdata.RightGraph, gdata.BottomGraph, gdata.black);
-	// diagonal line to close the right of the bottom depth
-	gdImageLine(gdata.im, gdata.RightGraph-10, gdata.BottomDepth, gdata.RightGraph, gdata.BottomGraph, gdata.black);
+	gdImageRectangle(gdata.im,LeftMargin,gdata.TopGraph,gdata.RightGraph,gdata.BottomGraph,gdata.gray);
+	gdImageLine(gdata.im,LeftMargin,gdata.BottomGraph,gdata.RightGraph,gdata.BottomGraph,gdata.dimgray);
+	gdImageLine(gdata.im,LeftMargin,gdata.TopGraph,LeftMargin,gdata.BottomGraph,gdata.dimgray);
 
 	// Y axis ticks
 	greport_draw_yaxis(pdata,&gdata);
@@ -598,7 +548,7 @@ static void greport_plot(const struct userinfostruct *
 					yval=-1.;
 					break;
 			}
-			if (yval>=0.) bar(&gdata,x1,yval,blabel);
+			if (yval>=0.) bar(&gdata,x1,yval,blabel,day);
 		}
 	}
 

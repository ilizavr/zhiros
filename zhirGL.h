Point points[] = {
        {200, 450},
        {600, 450},
        {600, 300},
        {400, 200},
        {200, 300},
};



vec2 centered;
int size = sizeof(points)/sizeof(points[0]); 
int x =0, y = 0;


void draw_figure() {

        centered = figure_center(points, size);

        x += 10;

        // vec2_rotate(points, size, centered, 0.2, 90.0f);
        vec2_locate(points, size, x, 300, 10);

        fill_polygon(points, size,  0xFFFFFF);
}

//исполняемая функция в kernel.c
void draw_some() {
        clearframe();
	draw_figure();
}
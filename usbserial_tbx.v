/*
    USB Serial

    Wrapping usb/usb_uart_ice40.v to create a loopback.
*/

module usbserial_tbx (
        input  clk_25mhz,

        inout  usb_fpga_dp,
        inout  usb_fpga_dn,
        output usb_fpga_pu_dp,

        output [7:0] led,

        // output [3:0] debug

        output wifi_gpio0
    );

    wire clk_48mhz;

    wire clk_locked;

    // Use an icepll generated pll
    pll pll48( .clkin(clk_25mhz), .clkout0(clk_48mhz), .locked( clk_locked ) );

    // LED
    reg [24:0] ledCounter;
    always @(posedge clk_48mhz) begin
        ledCounter <= ledCounter + 1;
    end
    // assign led = ledCounter[ 24:(24-8) ];

    // Generate reset signal
    reg [5:0] reset_cnt = 0;
    wire reset = ~reset_cnt[5];
    always @(posedge clk_48mhz)
        if ( clk_locked )
            reset_cnt <= reset_cnt + reset;

    // uart pipeline in
    wire [7:0] uart_in_data;
    wire       uart_in_valid;
    wire       uart_in_ready;

    // assign debug = { uart_in_valid, uart_in_ready, reset, clk_48mhz };

    wire usb_p_tx;
    wire usb_n_tx;
    wire usb_p_rx;
    wire usb_n_rx;
    wire usb_tx_en;

    // usb uart - this instanciates the entire USB device.
    usb_uart uart (
        .clk_48mhz  (clk_48mhz),
        .reset      (reset),

        // pins
        .pin_usb_p( usb_fpga_dp ),
        .pin_usb_n( usb_fpga_dn ),

        // uart pipeline in
        .uart_in_data( uart_in_data ),
        .uart_in_valid( uart_in_valid ),
        .uart_in_ready( uart_in_ready ),

        .uart_out_data( uart_in_data ),
        .uart_out_valid( uart_in_valid ),
        .uart_out_ready( uart_in_ready  )

        //.debug( debug )
    );

    // USB Host Detect Pull Up
    assign usb_fpga_pu_dp = 1'b1;

    // Tie GPIO0, keep board from rebooting
    assign wifi_gpio0 = 1'1;

    reg [7:0] led_r;
    assign led = led_r;
    always @(posedge clk_48mhz) begin
        if (uart_in_valid) begin
            led_r <= uart_in_data;
        end
    end
endmodule

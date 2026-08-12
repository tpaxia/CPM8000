// ============================================================================
// mmu.v - CP/M-8000 on Z8002 banking MMU
//
// Implements the two-tier memory-management unit specified in MEMORY_MODEL.md:
//
//   * Logical address  : 16 bits (64 KB) from the non-segmented Z8002.
//                        A[15:14] = chunk (C0..C3, four 16 KB chunks)
//                        A[13:0]  = offset (always passed through untranslated)
//   * Physical address : 19 bits (512 KB).
//                        P[18:16] = bank (8 x 64 KB); banks 0-3 = RAM (256 KB),
//                                   bank 4 = ROM, banks 5-7 reserved.
//                        P[15:14] = sub-chunk, P[13:0] = offset
//                        P[18:14] = 5-bit physical 16 KB chunk number
//
// Two register sets, selected by the CPU N/S status line:
//   NORMAL mode (TPA program): whole-64 KB, offset-preserving banking.
//       bank = I/D ? NBANK_I : NBANK_D ; P = {bank, A[15:14], A[13:0]}
//   SYSTEM mode (CCP/BDOS/BIOS): 16 KB paging with up to two movable apertures.
//       per-chunk SSEL bit picks A (identity into SHOME bank) or B (aperture).
//       The 1st (lowest-index) B chunk uses SAP0, the 2nd uses SAP1.
//
// Registers are written by privileged special-I/O (SOUT) and are only writable
// in system mode; normal-mode writes are ignored.  See §4/§9 of MEMORY_MODEL.md.
//
// Reset default (§8): system mode, SHOME = 4 (ROM bank), SSEL = 0 (all A), so
// every logical address is an identity map into the ROM bank and the Z8002
// reset-vector fetch lands in ROM.
// ============================================================================

`timescale 1ns / 1ps

module mmu (
    input             clk,
    input             rst_n,

    // Translation inputs (per bus cycle)
    input             sys_mode,     // 1 = system mode, 0 = normal mode
    input             id_instr,     // 1 = instruction fetch, 0 = data/stack ref
    input      [15:0] laddr,        // logical address (latched CPU address)
    output     [18:0] paddr,        // physical address

    // Register-write port (privileged special-I/O)
    input             reg_wr,       // write strobe (already qualified: special-I/O
                                    //   write cycle to an MMU port)
    input      [2:0]  reg_sel,      // which MMU register (see localparams below)
    input      [7:0]  reg_data      // write data (low bits used per register)
);

    // ------------------------------------------------------------------
    // Register file (23 bits total, MEMORY_MODEL.md §4)
    // ------------------------------------------------------------------
    reg [2:0] nbank_i;   // normal-mode instruction bank
    reg [2:0] nbank_d;   // normal-mode data bank
    reg [2:0] shome;     // system-mode home bank ("A" identity base)
    reg [3:0] ssel;      // per-chunk A/B select (bit i = chunk Ci: 0=A, 1=B)
    reg [4:0] sap0;      // aperture 0 physical 16 KB chunk#
    reg [4:0] sap1;      // aperture 1 physical 16 KB chunk#

    // Register selectors (special-I/O offsets, see HARDWARE.md)
    localparam SEL_NBANK_I = 3'd0;
    localparam SEL_NBANK_D = 3'd1;
    localparam SEL_SHOME   = 3'd2;
    localparam SEL_SSEL    = 3'd3;
    localparam SEL_SAP0    = 3'd4;
    localparam SEL_SAP1    = 3'd5;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            // Reset default: map every fetch into the ROM bank (4) in BOTH
            // register sets.  The Z8000 reset-vector fetch happens in normal
            // mode (FCW -- which selects system mode -- is loaded *from* the
            // vector), so the normal-mode banks must also point at ROM or the
            // vector fetch would read uninitialized RAM.  Software reprograms
            // NBANK_* before entering normal mode for a TPA.
            nbank_i <= 3'd4;      // ROM bank
            nbank_d <= 3'd4;      // ROM bank
            shome   <= 3'd4;      // ROM bank
            ssel    <= 4'd0;      // all A (identity into SHOME)
            sap0    <= 5'd0;
            sap1    <= 5'd0;
        end else if (reg_wr && sys_mode) begin
            // Privileged: only writable in system mode.
            case (reg_sel)
                SEL_NBANK_I: nbank_i <= reg_data[2:0];
                SEL_NBANK_D: nbank_d <= reg_data[2:0];
                SEL_SHOME:   shome   <= reg_data[2:0];
                SEL_SSEL:    ssel    <= reg_data[3:0];
                SEL_SAP0:    sap0    <= reg_data[4:0];
                SEL_SAP1:    sap1    <= reg_data[4:0];
                default: ;
            endcase
        end
    end

    // ------------------------------------------------------------------
    // Address translation (combinational, MEMORY_MODEL.md §5)
    // ------------------------------------------------------------------
    wire [1:0]  lchunk = laddr[15:14];   // logical chunk 0..3
    wire [13:0] off    = laddr[13:0];    // in-chunk offset (passthrough)

    // Normal mode: identity within the selected 64 KB bank.
    wire [2:0]  nbank  = id_instr ? nbank_i : nbank_d;
    wire [4:0]  nchunk = {nbank, lchunk};   // 3-bit bank : 2-bit sub-chunk

    // System mode: A = identity into SHOME, B = aperture (SAP0 / SAP1).
    // The 1st (lowest-index) B chunk binds to SAP0, the 2nd to SAP1.
    wire is_b = ssel[lchunk];
    // Count B-selected chunks with a lower index than the current chunk.
    // Only chunks strictly below lchunk matter for ordering.
    wire b0 = ssel[0];
    wire b1 = ssel[1];
    wire b2 = ssel[2];
    reg  b_before;   // is there a B chunk below the current one?
    always @(*) begin
        case (lchunk)
            2'd0: b_before = 1'b0;
            2'd1: b_before = b0;
            2'd2: b_before = b0 | b1;
            2'd3: b_before = b0 | b1 | b2;
            default: b_before = 1'b0;
        endcase
    end
    wire [4:0] aperture = b_before ? sap1 : sap0;   // 2nd B -> SAP1 else SAP0
    wire [4:0] schunk   = is_b ? aperture : {shome, lchunk};

    // Select set by mode.
    wire [4:0] pchunk = sys_mode ? schunk : nchunk;

    assign paddr = {pchunk, off};   // {P[18:14], A[13:0]}

endmodule

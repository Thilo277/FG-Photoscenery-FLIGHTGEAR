/***************************************************************************

    TITLE:                atmos_62

----------------------------------------------------------------------------

    FUNCTION:                1962 atmosphere table interpolation routine

----------------------------------------------------------------------------

    MODULE STATUS:        developmental

----------------------------------------------------------------------------

    GENEALOGY:        Created 920827 by Bruce Jackson as part of the C-castle
                development effort.

----------------------------------------------------------------------------

    DESIGNED BY:        B. Jackson

    CODED BY:                B. Jackson

    MAINTAINED BY:        B. Jackson

----------------------------------------------------------------------------

    MODIFICATION HISTORY:

        DATE        PURPOSE                                                        BY
        931220        Added ambient pressure and temperature as outputs.        EBJ
        940111        Changed includes from "ls_eom.h" to "ls_types.h" and
                "ls_constants.h"; changed DATA to SCALAR types.                EBJ

----------------------------------------------------------------------------

    REFERENCES:

    [ 1]        Hornbeck, Robert W.: "Numerical Methods", Prentice-Hall,
                1975.  ISBN  0-13-626614-2

----------------------------------------------------------------------------

    CALLED BY:

----------------------------------------------------------------------------

    CALLS TO:

----------------------------------------------------------------------------

    INPUTS:

----------------------------------------------------------------------------

    OUTPUTS:

--------------------------------------------------------------------------*/

#include "ls_types.h"
#include "ls_constants.h"

#include "atmos_62.h"

#include <math.h>

#define        alt_0        d_a_table[index  ][0]
#define        alt_1        d_a_table[index+1][0]
#define        den_0        d_a_table[index  ][1]
#define den_1        d_a_table[index+1][1]
#define        sps_0        d_a_table[index  ][2]
#define        sps_1        d_a_table[index+1][2]
#define gden_0        d_a_table[index  ][3]
#define        gden_1        d_a_table[index+1][3]
#define        gsps_0        d_a_table[index  ][4]
#define gsps_1        d_a_table[index+1][4]

#define MAX_ALT_INDEX 121
#define DELT_ALT 2000.
#define HLEV 36089.
#define TAMB0 518.7
#define PAMB0 2113.8
#define MAX_ALTITUDE 240000.

void ls_atmos( SCALAR altitude, SCALAR * sigma, SCALAR * v_sound, 
                SCALAR * t_amb, SCALAR * p_amb )
{

    int                index;
    SCALAR        daltp, daltn, daltp3, daltn3, density;
    SCALAR        t_amb_r, p_amb_r;
    SCALAR      tmp;

    static SCALAR        d_a_table[MAX_ALT_INDEX][5] =
    {
        {      0.,         2.37701E-03,         1.11642E+03,         0.00000E+00,         0.00000E+00        },
        {   2000.,         2.24098E-03,         1.10872E+03,         1.92857E-12,        -1.75815E-08        },
        {   4000.,         2.11099E-03,         1.10097E+03,         1.34570E-12,        -1.21740E-08        },
        {   6000.,         1.98684E-03,         1.09315E+03,         1.44862E-12,        -1.47225E-08        },
        {   8000.,         1.86836E-03,         1.08529E+03,         1.36481E-12,        -1.44359E-08        },
        {  10000.,         1.75537E-03,         1.07736E+03,         1.32716E-12,        -1.45340E-08        },
        {  12000.,         1.64768E-03,         1.06938E+03,         1.27657E-12,        -1.44280E-08        },
        {  14000.,         1.54511E-03,         1.06133E+03,         1.24656E-12,        -1.62540E-08        },
        {  16000.,         1.44751E-03,         1.05323E+03,         1.19220E-12,        -1.50560E-08        },
        {  18000.,         1.35469E-03,         1.04506E+03,         1.15463E-12,        -1.65220E-08        },
        {  20000.,         1.26649E-03,         1.03683E+03,         1.11926E-12,        -1.63562E-08        },
        {  22000.,         1.18276E-03,         1.02853E+03,         1.07333E-12,        -1.70533E-08        },
        {  24000.,         1.10333E-03,         1.02016E+03,         1.03743E-12,        -1.59305E-08        },
        {  26000.,         1.02805E-03,         1.01173E+03,         1.00195E-12,        -2.27248E-08        },
        {  28000.,         9.56760E-04,         1.00322E+03,         9.39764E-13,         3.29851E-10        },
        {  30000.,         8.89320E-04,         9.94641E+02,         1.01399E-12,        -8.80946E-08        },
        {  32000.,         8.25570E-04,         9.85988E+02,         5.39268E-13,         2.41048E-07        },
        {  34000.,         7.65380E-04,         9.77258E+02,         2.16894E-12,        -9.91599E-07        },
        {  36000.,         7.08600E-04,         9.68448E+02,        -4.10001E-12,         3.60535E-06        },
        {  38000.,         6.44190E-04,         9.68053E+02,         2.78612E-12,        -8.07290E-07        },
        {  40000.,         5.85146E-04,         9.68053E+02,         1.00455E-12,         2.16313E-07        },
        {  42000.,         5.31517E-04,         9.68053E+02,         1.31819E-12,        -5.79609E-08        },
        {  44000.,         4.82801E-04,         9.68053E+02,         1.09217E-12,         1.55309E-08        },
        {  46000.,         4.38554E-04,         9.68053E+02,         1.01661E-12,        -4.16262E-09        },
        {  48000.,         3.98359E-04,         9.68053E+02,         9.19375E-13,         1.11961E-09        },
        {  50000.,         3.61850E-04,         9.68053E+02,         8.34886E-13,        -3.15801E-10        },
        {  52000.,         3.28686E-04,         9.68053E+02,         7.58579E-13,         1.43600E-10        },
        {  54000.,         2.98561E-04,         9.68053E+02,         6.89297E-13,        -2.58597E-10        },
        {  56000.,         2.71197E-04,         9.68053E+02,         6.25735E-13,         8.90788E-10        },
        {  58000.,         2.46341E-04,         9.68053E+02,         5.69765E-13,        -3.30456E-09        },
        {  60000.,         2.23765E-04,         9.68053E+02,         5.15206E-13,         1.23274E-08        },
        {  62000.,         2.03256E-04,         9.68053E+02,         4.69912E-13,        -4.60052E-08        },
        {  64000.,         1.84627E-04,         9.68053E+02,         4.25146E-13,         1.71693E-07        },
        {  66000.,         1.67616E-04,         9.68314E+02,         2.56502E-13,        -2.49268E-07        },
        {  68000.,         1.51855E-04,         9.68676E+02,         4.23844E-13,         9.76878E-07        },
        {  70000.,         1.37615E-04,         9.71034E+02,         3.29621E-13,        -6.64245E-07        },
        {  72000.,         1.24744E-04,         9.72390E+02,         3.11170E-13,         1.77102E-07        },
        {  74000.,         1.13107E-04,         9.73745E+02,         2.76697E-13,        -4.56627E-08        },
        {  76000.,         1.02584E-04,         9.75099E+02,         2.53043E-13,         4.04902E-09        },
        {  78000.,         9.30660E-05,         9.76450E+02,         2.18633E-13,         2.49667E-08        },
        {  80000.,         8.44530E-05,         9.77799E+02,         2.29927E-13,        -1.06916E-07        },
        {  82000.,         7.67140E-05,         9.78950E+02,         1.72660E-13,         1.05696E-07        },
        {  84000.,         6.97010E-05,         9.80290E+02,         1.68432E-13,        -3.23682E-08        },
        {  86000.,         6.33490E-05,         9.81620E+02,         1.45113E-13,         8.77690E-09        },
        {  88000.,         5.75880E-05,         9.82950E+02,         1.37617E-13,        -2.73938E-09        },
        {  90000.,         5.23700E-05,         9.84280E+02,         1.18918E-13,         2.18061E-09        },
        {  92000.,         4.76350E-05,         9.85610E+02,         1.11210E-13,        -5.98306E-09        },
        {  94000.,         4.33410E-05,         9.86930E+02,         9.77408E-14,         6.75162E-09        },
        {  96000.,         3.94430E-05,         9.88260E+02,         9.18264E-14,        -6.02343E-09        },
        {  98000.,         3.59080E-05,         9.89580E+02,         7.94534E-14,         2.34210E-09        },
        { 100000.,         3.26960E-05,         9.90900E+02,         7.48600E-14,        -3.34498E-09        },
        { 102000.,         2.97810E-05,         9.92210E+02,         6.66067E-14,        -3.96219E-09        },
        { 104000.,         2.71320E-05,         9.93530E+02,         5.77131E-14,         3.41937E-08        },
        { 106000.,         2.46980E-05,         9.95410E+02,         2.50410E-14,         7.07187E-07        },
        { 108000.,         2.24140E-05,         9.99070E+02,         6.71229E-14,        -1.92943E-07        },
        { 110000.,         2.03570E-05,         1.00272E+03,         4.69675E-14,         4.95832E-08        },
        { 112000.,         1.85010E-05,         1.00636E+03,         4.65069E-14,        -2.03903E-08        },
        { 114000.,         1.68270E-05,         1.00998E+03,         4.00047E-14,         1.97789E-09        },
        { 116000.,         1.53150E-05,         1.01359E+03,         3.64744E-14,        -2.52130E-09        },
        { 118000.,         1.39480E-05,         1.01719E+03,         3.15976E-14,        -6.89271E-09        },
        { 120000.,         1.27100E-05,         1.02077E+03,         3.06351E-14,         9.21465E-11        },
        { 122000.,         1.15920E-05,         1.02434E+03,         2.58618E-14,        -8.47587E-09        },
        { 124000.,         1.05790E-05,         1.02789E+03,         2.34176E-14,         3.81135E-09        },
        { 126000.,         9.66010E-06,         1.03144E+03,         2.16178E-14,        -6.76951E-09        },
        { 128000.,         8.82710E-06,         1.03497E+03,         1.89611E-14,        -6.73330E-09        },
        { 130000.,         8.07070E-06,         1.03848E+03,         1.74377E-14,         3.70270E-09        },
        { 132000.,         7.38380E-06,         1.04199E+03,         1.55382E-14,        -8.07752E-09        },
        { 134000.,         6.75940E-06,         1.04548E+03,         1.41595E-14,        -1.39263E-09        },
        { 136000.,         6.19160E-06,         1.04896E+03,         1.27239E-14,        -1.35196E-09        },
        { 138000.,         5.67490E-06,         1.05243E+03,         1.15951E-14,        -8.19953E-09        },
        { 140000.,         5.20450E-06,         1.05588E+03,         1.03459E-14,         4.15010E-09        },
        { 142000.,         4.77570E-06,         1.05933E+03,         9.42149E-15,        -8.40086E-09        },
        { 144000.,         4.38470E-06,         1.06276E+03,         8.66820E-15,        -5.46671E-10        },
        { 146000.,         4.02820E-06,         1.06618E+03,         7.65573E-15,        -4.41246E-09        },
        { 148000.,         3.70260E-06,         1.06959E+03,         7.05890E-15,         3.19650E-09        },
        { 150000.,         3.40520E-06,         1.07299E+03,         6.40867E-15,        -2.33736E-08        },
        { 152000.,         3.13330E-06,         1.07637E+03,         5.55641E-15,         6.02977E-08        },
        { 154000.,         2.88480E-06,         1.07975E+03,         6.46568E-15,        -2.17817E-07        },
        { 156000.,         2.66270E-06,         1.08202E+03,         8.18087E-15,        -8.54029E-07        },
        { 158000.,         2.46830E-06,         1.08202E+03,         2.36086E-15,         2.28931E-07        },
        { 160000.,         2.28810E-06,         1.08202E+03,         3.67571E-15,        -6.16972E-08        },
        { 162000.,         2.12120E-06,         1.08202E+03,         2.88632E-15,         1.78573E-08        },
        { 164000.,         1.96640E-06,         1.08202E+03,         2.92903E-15,        -9.73206E-09        },
        { 166000.,         1.82300E-06,         1.08202E+03,         2.49757E-15,         2.10709E-08        },
        { 168000.,         1.69000E-06,         1.08202E+03,         2.68069E-15,        -7.45517E-08        },
        { 170000.,         1.56680E-06,         1.08202E+03,         1.47966E-15,         2.77136E-07        },
        { 172000.,         1.45250E-06,         1.08202E+03,         4.75068E-15,        -1.03399E-06        },
        { 174000.,         1.35240E-06,         1.07963E+03,         8.17622E-16,         2.73830E-07        },
        { 176000.,         1.25880E-06,         1.07723E+03,         1.72883E-15,        -7.63301E-08        },
        { 178000.,         1.17130E-06,         1.07482E+03,         1.41704E-15,         1.64901E-08        },
        { 180000.,         1.08960E-06,         1.07240E+03,         1.30299E-15,        -4.63038E-09        },
        { 182000.,         1.01320E-06,         1.06998E+03,         1.32100E-15,         2.03140E-09        },
        { 184000.,         9.41950E-07,         1.06756E+03,         1.13799E-15,        -3.49522E-09        },
        { 186000.,         8.75370E-07,         1.06513E+03,         1.13202E-15,        -3.05052E-09        },
        { 188000.,         8.13260E-07,         1.06269E+03,         1.03892E-15,         6.97283E-10        },
        { 190000.,         7.55320E-07,         1.06025E+03,         9.67290E-16,         2.61383E-10        },
        { 192000.,         7.01260E-07,         1.05781E+03,         9.11920E-16,        -1.74281E-09        },
        { 194000.,         6.50850E-07,         1.05536E+03,         8.60032E-16,        -8.29013E-09        },
        { 196000.,         6.03870E-07,         1.05290E+03,         7.92951E-16,         1.99033E-08        },
        { 198000.,         5.60110E-07,         1.05044E+03,         7.98164E-16,        -7.13232E-08        },
        { 200000.,         5.19320E-07,         1.04798E+03,         4.69394E-16,         2.65389E-07        },
        { 202000.,         4.81340E-07,         1.04550E+03,         1.53926E-15,        -1.02023E-06        },
        { 204000.,         4.47960E-07,         1.04063E+03,         2.73571E-16,         2.30547E-07        },
        { 206000.,         4.16690E-07,         1.03565E+03,         5.31456E-16,        -6.69551E-08        },
        { 208000.,         3.87320E-07,         1.03065E+03,         4.50605E-16,         7.27308E-09        },
        { 210000.,         3.59790E-07,         1.02562E+03,         4.26126E-16,        -7.13720E-09        },
        { 212000.,         3.33970E-07,         1.02057E+03,         4.09893E-16,        -8.72426E-09        },
        { 214000.,         3.09780E-07,         1.01549E+03,         3.79301E-16,        -2.96576E-09        },
        { 216000.,         2.87120E-07,         1.01039E+03,         3.67902E-16,        -9.41272E-09        },
        { 218000.,         2.65920E-07,         1.00526E+03,         3.39092E-16,        -4.38337E-09        },
        { 220000.,         2.46090E-07,         1.00011E+03,         3.30732E-16,        -3.05378E-09        },
        { 222000.,         2.27570E-07,         9.94940E+02,         3.02981E-16,        -1.34015E-08        },
        { 224000.,         2.10270E-07,         9.89730E+02,         2.87343E-16,        -3.34027E-09        },
        { 226000.,         1.94120E-07,         9.84500E+02,         2.72646E-16,        -3.23743E-09        },
        { 228000.,         1.79060E-07,         9.79250E+02,         2.57074E-16,        -1.37100E-08        },
        { 230000.,         1.65030E-07,         9.73960E+02,         2.44060E-16,        -1.92258E-09        },
        { 232000.,         1.51970E-07,         9.68650E+02,         2.21687E-16,        -8.59969E-09        },
        { 234000.,         1.39810E-07,         9.63310E+02,         2.19191E-16,        -8.67865E-09        },
        { 236000.,         1.28510E-07,         9.57940E+02,         1.91549E-16,        -1.68569E-09        },
        { 238000.,         1.18020E-07,         9.52550E+02,         2.29613E-16,        -1.45786E-08        },
        { 240000.,         1.08270E-07,         9.47120E+02,         0.00000E+00,         0.00000E+00        }
    };

    /* for purposes of doing the table lookup, force the incoming
       altitude to be >= 0 */

    // printf("altitude = %.2f\n", altitude);

    if ( altitude < 0.0 ) {
        altitude = 0.0;
    }

    // printf("altitude = %.2f\n", altitude);

    index = (int)( altitude / 2000 );
    if (index > (MAX_ALT_INDEX-2))
    {
     index = MAX_ALT_INDEX-2; /* limit maximum altitude */
     altitude = MAX_ALTITUDE;
    }
    if (index < 0) index = 0;
    daltp = alt_1 - altitude;
    daltp3 = daltp*daltp*daltp;
    daltn = altitude - alt_0;
    daltn3 = daltn*daltn*daltn;
    
    density = (gden_0/6)*((daltp3/2000) - 2000*daltp)
                            + (gden_1/6)*((daltn3/2000) - 2000*daltn)
                            + den_0*daltp/2000 + den_1*daltn/2000;
                            
    *v_sound = (gsps_0/6)*((daltp3/2000) - 2000*daltp)
                            + (gsps_1/6)*((daltn3/2000) - 2000*daltn)
                            + sps_0*daltp/2000 + sps_1*daltn/2000;

    *sigma = density/SEA_LEVEL_DENSITY;

    if (altitude < HLEV)    /* BUG - these curve fits are only good to about 75000 ft */
      {
        t_amb_r = 1. - 6.875e-6*altitude;
        // printf("index = %d  t_amb_r = %.2f\n", index, t_amb_r);
        // p_amb_r = pow( t_amb_r, 5.256 );
        tmp = 5.256; // avoid a segfault (?)
        p_amb_r = pow( t_amb_r, tmp );
        // printf("p_amb_r = %.2f\n", p_amb_r);
      }
    else
      {
        t_amb_r = 0.751895;
        p_amb_r = 0.2234*exp( -4.806e-5 * (altitude - HLEV));
      }

    *p_amb = p_amb_r * PAMB0;
    *t_amb = t_amb_r * TAMB0;

/* end of atmos_62 */
}
/**************************************************************************/

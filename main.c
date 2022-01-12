/*
Name: Reflex Game
Author: Yusuf Serdar Caliskan
Started at date: 26.12.2021
Last update: 6.01.2022

CONNECTIONS
P2.0 -> LCD D4
P2.1 -> LCD D5
P2.2 -> LCD D6
P2.3 -> LCD D7
P2.4 -> LCD E
P2.5 -> LCD RS
P1.0 -> NULL (reserved for embedded red light)
P1.1 -> PLAYER 1 green BUTTON
P1.2 -> PLAYER 1 red BUTTON
P1.3 -> PLAYER 2 green BUTTON
P1.4 -> PLAYER 2 red BUTTON
P1.6 -> NULL (reserved for embedded green light)
3.3 VOLT to breadboard
GRD to breadboard
RW to GRND, D0 to D3 no connection
P1.7 -> green led (p1.0 for internal light then change related parts bellow in code. search "internal" to find)
P1.6 -> red led


Notes to self:
1_partialy done:  creating a new delay function which puts MCU in LPM could be great, extra features!



*/
#include <msp430g2553.h>
#include "lcdLib.h" //lcd lib from: http://www.ece.utep.edu/courses/web3376/Lab_5_-_LCD.html
#include <stdlib.h> //lib for random number and abs()
#include <time.h>   //lib for random number

//DISABLED SINCE I DONT USE THESE INSTEAD USE LPM DELAY FUNCTION I WROTE
//#define delay_s(x)     __delay_cycles((long) x* 1000000)
//#define delay_ms(x)     __delay_cycles((long) x* 1000)
//#define delay_s(x)     __delay_cycles(1)

void game();    //main game function which is 1 round
void writeScores(); //to display scores of round
void delay_ms_LPM(unsigned int);    //low power delay mode which wakes up every 1 ms


//******
//**********SETTINGS**********
#define numberOfRounds 3   //located at main loop
#define ledDelayLength 1000 //in us    //located at main loop
#define roundLength 2000   //in us //located in timer interrupt
#define delayBetweenRounds 4000 //in us    //located at main loop
#define lengthOfEndGameScore 5000 //in us    //located at main loop
#define lengthOfBestTime 5000 //in us    //located at main loop
#define lengthStartNew 3000 //in us    //located at main loop


unsigned volatile int scoreTime = 0;
char volatile gameBool = 0x00; //0: isGameOn 1: p1 correct 2: p2 correct 3: p1 wrong 4: p2 wrong 5: p1 timed-out 6: p2 timed-out 7: 0->green light 1->red light
char volatile gameBool2 = 0x00; //0: P1 pressed early 1: P2 pressed early
char volatile buttonBool = 0x00; //did button got pressed? from 1 to 4
unsigned short gameCount = 0;
unsigned volatile int player1Time;
unsigned volatile int player2Time;
unsigned volatile int player1Score;
unsigned volatile int player2Score;
unsigned volatile int p1Best;
unsigned volatile int p2Best;
unsigned int randomNum;
unsigned volatile int LPM_delay = 0; //low power mode delay (wakes up every 1ms)






/**
 * main.c
 */
void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    //Clock settings
    BCSCTL1 = CALBC1_1MHZ;                    // 1MHz clock
    DCOCTL = CALDCO_1MHZ;
    IFG1 &= ~OFIFG; // Set Interrupt Flag Generator
    BCSCTL2 |= SELM_1;



    //Timer A control register
    TACTL = TASSEL_2 + MC_1;           // SMCLK, upmode //_BIS_SR(GIE); to enable interrupts
    CCR0 =  1000;
    CCTL0 = CCIE;                      // CCR0 interrupt enabled

    //LCD outputs
    P2DIR |= BIT0 + BIT1 + BIT2 + BIT3+ BIT4+ BIT5;

    //LED outputs
    P1DIR |=  BIT6;
    //P1DIR |= BIT0;      //enable for internal light
    //P1REN |= BIT0;      //enable for internal light (without this it doesn't shine. probably noise from timerA)
    P1DIR |= BIT7;        //disable for internal light

    //BUTTON CONFIG
    P1DIR &= ~BIT1 + ~BIT2 + ~BIT3 + ~BIT4; //buttons as input
    P1REN |= BIT1 + BIT2 + BIT3 + BIT4;
    P1IE |= BIT1 + BIT2 + BIT3 + BIT4;  //enable interrupts
    P1IES |= BIT1 + BIT2 + BIT3 + BIT4; //high to low selection
    P1OUT |= BIT1 + BIT2 + BIT3 + BIT4;
    P1OUT &= ~BIT6;
    //P1OUT  &= ~BIT0;  //re enable for internal led
    P1OUT  &= ~BIT7;    //disable for internal led
    P1IFG &= ~BIT1 + ~BIT2 + ~BIT3 + ~BIT4;

    srand(time(NULL)); //random intiallazion


    lcdInit();  //gotta run once to intialize 16x2 lcd

    //MAIN LOOP
    while(1) {
        lcdClear();
        lcdSetText("Reflex Game",0,0);
        lcdSetText("Starting new",0,1);

        player1Score=0;
        player2Score=0;
        p1Best = (roundLength+5);   //this is done so i can compare and change it while assigning it and don't write it if player didn't pressed at all
        p2Best = (roundLength+5);   //^without using and other variable
        gameCount=numberOfRounds;    //game count set

        delay_ms_LPM(lengthStartNew);

        //ROUNDS
        while(gameCount != 0) { //main game loop, will call the function every "delayBetweenRounds" until gameCount fulfilled. note: only rounds with valid
            game();             //^winners will reduce the gameCount
            delay_ms_LPM(delayBetweenRounds);
        }

        //game end scoring
        lcdClear();
        if(player1Score>player2Score) {
            lcdSetText("Player 1 WON",0,0);
            lcdSetText("P1:    P2: ",0,1);
            lcdSetInt(player1Score,3,1);
            lcdSetInt(player2Score,10,1);
        }
        else if(player1Score<player2Score) {
            lcdSetText("Player 2 WON",0,0);
            lcdSetText("P1:    P2: ",0,1);
            lcdSetInt(player1Score,3,1);
            lcdSetInt(player2Score,10,1);
        }
        delay_ms_LPM(lengthOfEndGameScore);


        //best response time write
        if(p1Best == (roundLength+5)) {
            lcdSetText("P1 best: NULL",0,0);
        }
        else {
            lcdSetText("P1 best:     ms",0,0);
            lcdSetInt(p1Best,9,0);
        }
        if(p2Best == (roundLength+5)) {
            lcdSetText("P2 best: NULL",0,1);
        }
        else {
            lcdSetText("P2 best:     ms",0,1);
            lcdSetInt(p2Best,9,1);
        }
        delay_ms_LPM(lengthOfBestTime);

    }


}







void game() {
    lcdClear();
    lcdSetText("****************",0,0);
    randomNum=3;    //to punish those pressed before led
    gameBool |= BIT0; //gameOn    (to start game before led disable bellow, enable this line)
    _BIS_SR(GIE);   //interrupts enabled

    delay_ms_LPM(ledDelayLength);
    _BIS_SR(GIE);//re enable after delay closes it


    //RANDOM LED*********
    randomNum = rand()%2;   //1=green
    if(randomNum==0) {
    //if(1) {
        gameBool &= ~BIT7;//green
        //P1OUT |= BIT0;    //enable for internal led
        P1OUT |= BIT7;  //disable for internal led
    }
    else {
        gameBool |= BIT7;//red
        P1OUT |= BIT6;
    }



    scoreTime=0;
    //gameBool |= BIT0; //gameOn    (to start game after led disable above, enable this line)

    while((gameBool & BIT0)==BIT0) {
        _BIS_SR(CPUOFF);  //LMP enabled
    }

    //close leds
    //P1OUT &= ~BIT0;   //enable for internal led
    P1OUT &= ~BIT7;     //disable for internal led
    P1OUT &= ~BIT6;

    writeScores();  //writes times to lcd and reducts gameCount if there's a winner

    //best time capture
    if(player1Time != 0) {
        if(player1Time < p1Best) p1Best= player1Time;
    }
    if(player2Time != 0) {
        if(player2Time < p2Best) p2Best= player2Time;
    }

    //Re-set variables
    gameBool = 0x00;
    gameBool2 = 0x00;
    buttonBool = 0x00;
    scoreTime = 0;
    player1Time=0;
    player2Time=0;
}






void writeScores() {   //write scores of ROUND
    lcdClear();
    if( ((gameBool & BIT1)!=0) && ((gameBool & BIT2)!=0)  ) { //P1 correct P2 correct
        if(player2Time==player1Time) {
            lcdSetText("WOW, ITS DRAW",0,0);    //not sure if this is even possible
            lcdSetText("Scored:      ms",0,1);
            lcdSetInt(player1Time,8,1);
        }
        else if(player2Time>player1Time) {
            lcdSetText("P1 WON:      ms ",0,0);
            lcdSetText("(    ms) faster",0,1);
            lcdSetInt(player1Time,8,0);
            lcdSetInt( abs(player1Time-player2Time) ,1,1);
            player1Score++;
            gameCount--;
        }
        else if(player2Time<player1Time) {
            lcdSetText("P2 WON:      ms ",0,0);
            lcdSetText("(    ms) faster",0,1);
            lcdSetInt(player2Time,8,0);
            lcdSetInt( abs(player2Time-player1Time) ,1,1);
            player2Score++;
            gameCount--;
        }
    }
    else if( ((gameBool & BIT1)!=0) && ((gameBool & BIT4)!=0)  ) {    //P1 correct P2 wrong
        lcdSetText("P1 WON:      ms ",0,0);
        lcdSetText("P2 miss-clicked",0,1);
        lcdSetInt(player1Time,8,0);
        player1Score++;
        gameCount--;
    }
    else if( ((gameBool & BIT2)!=0) && ((gameBool & BIT3)!=0)  ) {    //P1 wrong P2 correct
        lcdSetText("P2 WON:      ms ",0,0);
        lcdSetText("P1 miss-clicked",0,1);
        lcdSetInt(player2Time,8,0);
        player2Score++;
        gameCount--;
    }
    else if( ((gameBool & BIT1)!=0) && ((gameBool & BIT6)!=0)  ) {    //P1 correct P2 timed-out
        lcdSetText("P1 WON:      ms ",0,0);
        lcdSetText("P2 timed-out",0,1);
        lcdSetInt(player1Time,8,0);
        player1Score++;
        gameCount--;
    }
    else if( ((gameBool & BIT2)!=0) && ((gameBool & BIT5)!=0)  ) {    //P1 timed-out P2 correct
        lcdSetText("P2 WON:      ms ",0,0);
        lcdSetText("P1 timed-out",0,1);
        lcdSetInt(player2Time,8,0);
        player2Score++;
        gameCount--;
    }
    else if( ((gameBool & BIT5)!=0) && ((gameBool & BIT6)!=0)  ) {    //both timed-out
        lcdSetText("No winners!",0,0);
        lcdSetText("Timed-out 2 sec",0,1);
    }
    else if( ((gameBool & BIT3)!=0) && ((gameBool & BIT6)!=0)  ) {    //p1 wrong p2 timed-out
        lcdSetText("P1 pressed wrong",0,0);
        lcdSetText("P2 timed-out",0,1);
    }
    else if( ((gameBool & BIT5)!=0) && ((gameBool & BIT4)!=0)  ) {    //p1 timed-out p2 wrong
        lcdSetText("P2 pressed wrong",0,0);
        lcdSetText("P1 timed-out",0,1);
    }
    else if( ((gameBool & BIT3)!=0) && ((gameBool & BIT4)!=0)  ) {    //both wrong
        lcdSetText("Both players",0,0);
        lcdSetText("pressed wrong",0,1);
    }
    else if( ((gameBool & BIT1)!=0) && ((gameBool2 & BIT1)!=0)  ) {    //p1 correct, p2 pressed early
        lcdSetText("P1 WON:      ms ",0,0);
        lcdSetText("P2 clicked early",0,1);
        lcdSetInt(player1Time,8,0);
        player1Score++;
        gameCount--;
    }
    else if( ((gameBool & BIT2)!=0) && ((gameBool2 & BIT0)!=0)  ) {    //p2 correct, p1 pressed early
        lcdSetText("P2 WON:      ms ",0,0);
        lcdSetText("P1 clicked early",0,1);
        lcdSetInt(player2Time,8,0);
        player2Score++;
        gameCount--;
    }
    else if( ((gameBool & BIT3)!=0) && ((gameBool2 & BIT1)!=0)  ) {    //p1 wrong, p2 pressed early
        lcdSetText("P1 pressed wrong",0,0);
        lcdSetText("P2 clicked early",0,1);
    }
    else if( ((gameBool & BIT4)!=0) && ((gameBool2 & BIT0)!=0)  ) {    //p2 wrong, p1 pressed early
        lcdSetText("P1 clicked early",0,0);
        lcdSetText("P2 pressed wrong",0,1);
    }
    else if( ((gameBool & BIT5)!=0) && ((gameBool2 & BIT1)!=0)  ) {    //p1 timed out, p2 pressed early
        lcdSetText("P1 timed-out",0,0);
        lcdSetText("P2 clicked early",0,1);
    }
    else if( ((gameBool & BIT6)!=0) && ((gameBool2 & BIT0)!=0)  ) {    //p2 timed out, p1 pressed early
        lcdSetText("P1 clicked early",0,0);
        lcdSetText("P2 timed-out",0,1);
    }
    else if( ((gameBool2 & BIT1)!=0) && ((gameBool2 & BIT0)!=0)  ) {    //both pressed early
        lcdSetText("Both players",0,0);
        lcdSetText("pressed early",0,1);
    }
    else {  //added in case something went wrong above, might delete this as it stands useless since i wrote all possible 16 situations above
        lcdSetText("Invalid result",0,0);
        lcdSetText("Starting new",0,1);
    }


}







void delay_ms_LPM(unsigned int delayTime) { //low power delay function. not so optimized as it wakes up every 1ms, can improve this actually to be
    LPM_delay = delayTime;                  //^low power delay function. could be done by changing CCR0 but gotta look more into it.
    _BIS_SR(CPUOFF + GIE);
}








#pragma vector=TIMER0_A0_VECTOR //timer interrupt for ms counting
__interrupt void Timer_A (void)
{

    if(LPM_delay!=0) {  //is there LPM delay?
        LPM_delay = LPM_delay - 1;
        if(LPM_delay==0) {
            __bic_SR_register_on_exit(CPUOFF + GIE);
        }
    }


    if( ((gameBool & BIT0)!=0)&& (randomNum!=3) ) {
        scoreTime++;
       if(scoreTime>roundLength) {
           if( ((gameBool & BIT1)==0)&&((gameBool & BIT3)==0)&&((gameBool2 & BIT0)==0)   ) { //did player 1 timed-out
               gameBool |= BIT5;
           }
           if( ((gameBool & BIT2)==0)&&((gameBool & BIT4)==0)&&((gameBool2 & BIT1)==0)   ) { //did player 2 timed-out
               gameBool |= BIT6;
           }
           gameBool &= ~BIT0;
           __bic_SR_register_on_exit(CPUOFF + GIE);  //LMP and interrupts disabled

       }
    }
}








#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    //led is off
    if(randomNum==3) {
        if( ((P1IN & BIT1) == 0) || ((P1IN & BIT2) == 0) ) {    //if player 1 is early
            gameBool2 |= BIT0;
            buttonBool |= BIT1;
            buttonBool |= BIT2;
        }
        if( ((P1IN & BIT3) == 0) || ((P1IN & BIT4) == 0) ) {    //if player 1 is early
            gameBool2 |= BIT1;
            buttonBool |= BIT3;
            buttonBool |= BIT4;
        }
    }


    //led is on
    if((gameBool & BIT0)== BIT0  ) { //is game on?
        if( ((P1IN & BIT1) == 0) && ((buttonBool & BIT1) == 0) ) {    //p1 green
            if( (gameBool&BIT7) == BIT7  ) {    //game lose
                gameBool |= BIT3;
                gameBool &= ~BIT1;
                player1Time = 0;
                buttonBool |= BIT2;
            }
            else if((gameBool&BIT3)==0){   //game win
                gameBool |= BIT1;
                player1Time = scoreTime;
            }
            buttonBool |= BIT1;
        }

        if( ((P1IN & BIT2) == 0) && ((buttonBool & BIT2) == 0) ) {    //p1 red
            if( (gameBool&BIT7) != BIT7  ) {    //game lose
                gameBool |= BIT3;
                gameBool &= ~BIT1;
                player1Time = 0;
                buttonBool |= BIT1;
            }
            else if((gameBool&BIT4)==0){   //game win
                gameBool |= BIT1;
                player1Time = scoreTime;
            }
            buttonBool |= BIT2;
        }

        if( ((P1IN & BIT3) == 0) && ((buttonBool & BIT3) == 0) ) {    //p2 green
            if( (gameBool&BIT7) == BIT7  ) {    //game lose
                gameBool |= BIT4;
                gameBool &= ~BIT2;
                player2Time = 0;
                buttonBool |= BIT4;
            }
            else{   //game win
                gameBool |= BIT2;
                player2Time = scoreTime;
            }
            buttonBool |= BIT3;
        }

        if( ((P1IN & BIT4) == 0) && ((buttonBool & BIT4) == 0) ) {    //p2 red
            if( (gameBool&BIT7) != BIT7  ) {    //game lose
                gameBool |= BIT4;
                gameBool &= ~BIT2;
                player2Time = 0;
                buttonBool |= BIT3;
            }
            else{   //game win
                gameBool |= BIT2;
                player2Time = scoreTime;
            }
            buttonBool |= BIT4;
        }
    }


    P1IFG = 0x00;
}

/* This code aims to direct gpio 0 as output and gpio 2 as input.
 * The two pins wil be connected together and send output when pins detect each others.
 */

#include <stdio.h>
#include <gpiod.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef CONSUMER
#define CONSUMER    "YuSheng"
#endif

int pwm_num = 0;

void *generate_pwm_random_interval( void *ptr );
void *detect_pwm( void *ptr );

int main(int argc, char const *argv[])
{
    pthread_t thread1, thread2;
    char *chipname = "gpiochip0";
	static unsigned int line_num1 = 0;
    static unsigned int line_num2 = 2;
	struct gpiod_chip *chip;
	struct gpiod_line *line1;
    struct gpiod_line *line2;
    int i, ret, iret1, iret2;

    chip = gpiod_chip_open_by_name(chipname);
	if (!chip) {
		perror("Open chip failed\n");
		ret = -1;
		goto end;
	}

    line1 = gpiod_chip_get_line(chip, line_num1);
    if (!line1) {
		perror("Error! Failed to get line 1\n");
		ret = -1;
		goto close_chip;
	}

    line2 = gpiod_chip_get_line(chip, line_num2);
    if (!line2) {
		perror("Error! Failed to get line 2\n");
		ret = -1;
		goto release_line1;
	}

    ret = gpiod_line_request_output(line1, CONSUMER, 0);
    if (ret < 0) {
		perror("Error! Failed to request output at line 1\n");
		ret = -1;
		goto release_line2;
	}

    ret = gpiod_line_request_both_edges_events(line2, CONSUMER);
    if (ret < 0) {
		perror("Error! Failed to request input at line 2\n");
		ret = -1;
		goto release_line2;
	}

    //create 2 thread, one send output, one received input
    //join both threads
    iret1 = pthread_create( &thread1, NULL, generate_pwm_random_interval, (void*) line1);
    if (iret1 < 0) {
		perror("Error! Failed to generate pwm at pin 0\n");
		ret = -1;
		goto release_line2;
	}
    iret2 = pthread_create( &thread2, NULL, detect_pwm, (void*) line2);
    if (iret1 < 0) {
		perror("Error! Failed to generate pwm at pin 0\n");
		ret = -1;
		goto release_line2;
	}

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

release_line2:
    gpiod_line_release(line2);
release_line1:
    gpiod_line_release(line1);
close_chip:
    gpiod_chip_close(chip);
end:
    return ret;
}

void *generate_pwm_random_interval( void *ptr ){
    int i, ret, val = 0, timing;
    struct gpiod_line *line = ptr;

    for (i = 0; i < 10; i++){
        val = !val;
        printf("%d ", val);
        pwm_num++;
        ret = gpiod_line_set_value(line, val);
        timing = (rand()%10 + 1)*100000;            //generate timing of 0.1s, 0.2s, 0.3s...1s
        usleep(timing);
    }
}

void *detect_pwm( void *ptr ){
    int i, ret, val = 0, timing, counter = 0;
    struct gpiod_line *line = ptr;
    bool stp = false;
    struct timespec ts = { 3, 0 };
    struct gpiod_line_event event;

    while (true) {
		ret = gpiod_line_event_wait(line, &ts);
		// printf("%d \t", ret);
		if (ret < 0) {
			perror("Wait event notification failed\n");
            printf("fail");
			ret = -1;
			stp = true;
		} else if (ret == 0) {
			printf("FAILED! Wait event notification on line #%u timeout\n", line);
			break;
		}

		ret = gpiod_line_event_read(line, &event);
		if (ret < 0) {
			perror("Read last event notification failed\n");
			stp = true;
		}

        switch (event.event_type)
        {
        case GPIOD_LINE_EVENT_RISING_EDGE:{
            printf("Rising edge detected \n");
            counter++;
            break;
            }

        case GPIOD_LINE_EVENT_FALLING_EDGE:{
            printf("Falling edge detected\n");
            counter++;
            break;
            }

        default:{
            }
        }

        if (stp == true){
            printf("break");
            break;
        }

        if (pwm_num == 10 && counter == 10){
            printf("10 signal detected successfully\n");
            break;
        }
        else if (pwm_num == 10 && counter != 10)
        {
            printf("FAILED! %d signal detected\n", counter);
            break;
        }
	}
}

/*
 * Copyright (c) 2007 Cisco, Inc. All rights reserved.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "otpo.h"

void handler (int);
static int get_num_combinations (int);
static int set_mca_options (int, char**);
static void print_usage (void);
int stop_signal;
time_t t1, t2, et1, et2;

int main(int argc , char *argv[]) 
{
    /*otpo_param_list_t *list_params = NULL;*/
    int num_parameters, i, k, num_comb, hr, min, sec;
    int num_tested, current_winner, resume, num_functions;
    char *input_file = NULL, *output_dir = NULL;
    char *interrupt_file = NULL;
    float current;
    double *results = NULL, *unfiltered = NULL, *outliers = NULL;
    ADCL_Attribute *ADCL_param_attributes = NULL;
    ADCL_Attrset ADCL_param_attrset;
    ADCL_Fnctset ADCL_param_fnctset;
    ADCL_Topology ADCL_param_topo;
    ADCL_Request ADCL_param_request;
    /* options */
    static struct option long_opts[] = 
    {
        { "params", required_argument, NULL, 'p' },
        { "test", required_argument, NULL, 't' },
        { "test_path", required_argument, NULL, 'w' },
        { "verbose", no_argument, NULL, 'v' },
        { "status", no_argument, NULL, 's' },
        { "debug", no_argument, NULL, 'd' },
        { "silent", no_argument, NULL, 'n' },
        { "message_length", required_argument, NULL, 'l' }, 
        { "hostfile", required_argument, NULL, 'h' },
        { "mca", required_argument, NULL, 'm' },
        { "format", required_argument, NULL, 'f' },
        { "out", required_argument, NULL, 'o' },
        { "backup", required_argument, NULL, 'b' },
        { "resume", required_argument, NULL, 'r' },
        { NULL, 0, NULL, 0 }
    };

    et1 = time (NULL);
    stop_signal = 0;

    verbose = 0;
    status = 0;
    debug = 0;
    mca_args_len = 0;
    hostf = NULL;
    test = NULL;
    test_path = NULL;
    msg_size = NULL;
    resume = 0;

    if (SUCCESS != set_mca_options (argc, argv)) 
    {
        fprintf(stderr, "Invalid mca options");
        exit(1);
    }

    while (-1 != (i = getopt_long (argc, argv, "p:t:w:vsndl:m:h:g:o:f:r:b:", long_opts, NULL))) 
    {
        switch (i) 
        {
        case 'p':
            input_file = strdup (optarg);
            break;
        case 'r':
            resume = 1;
            interrupt_file = strdup (optarg);
            break;
        case 'b':
            interrupt_file = strdup (optarg);
            break;
        case 'f':
            if (strcasecmp (optarg, "XML")) 
            {
                output = XML;
            }
            else 
            {
                output = TEXT;
            }
            break;
        case 'v':
            verbose = 1;
            break;
        case 's':
            status = 1;
            break;
        case 'n':
            break;
        case 'd':
            debug = 1;
            break;
        case 't':
            test = strdup (optarg);
            if (strcasecmp (test,"Netpipe"))
            {
                printf ("Invalid Test Name\n");
                exit (1);
            }
            break;
        case 'w':
            test_path = strdup (optarg);
            break;
        case 'l':
            msg_size = strdup (optarg);
            if (optarg[0] != '0' && atoi (optarg) == 0) 
            {
                printf ("Invalid Message Size\n");
                exit (1);
            }
            break;
        case 'm':
            break;
        case 'h':
            hostf = strdup (optarg);
            break;
        case 'o':
            output_dir = strdup (optarg);
            break;
        default: 
            print_usage();
            exit (1);
        }
    }

    if (NULL == input_file)
    {
        printf ("You need an input file that contains the parameters\n");
        print_usage();
        exit (1);
    }

    if (NULL == test_path)
    {
        printf ("You need to specify the path to the test u want to execute\n");
        print_usage();
        exit (1);
    }

    if (NULL == output_dir)
    {
        output_dir = strdup ("results");
    }

    if (NULL == interrupt_file)
    {
        interrupt_file = strdup ("interrupt.txt");
    }

    if (NULL == msg_size)
    {
        msg_size = strdup ("1");
    }

    if (NULL == test)
    {
        test = strdup ("Netpipe");
    }

    if (verbose || debug)
    {
        printf ("I will read the Parameter from %s\n", input_file);
        printf ("I will write the intermediate results to %s\n", output_dir);
        printf ("In case I detect an interrupt, I will write my data to %s\n", interrupt_file);
        printf ("The Test case will be using %s, with %s byte messages\n",test,msg_size); 
    }

    /* read file to get number of parameters */
    if (0 == (num_parameters = otpo_get_num_parameters (input_file))) 
    {
        printf ("File contains no Data\n");
        exit (0);
    }
    if (debug) 
    {
        printf ("Read %d parameters\n",num_parameters);
    }

    /* structure that hold all the parameter info */
    list_params = (struct otpo_param_list_t *)
        malloc(sizeof(struct otpo_param_list_t) * num_parameters);
    if (NULL == list_params) 
    {
        fprintf (stderr,"Malloc Failed...\n");
        exit (1);
    }

    /* initialize list_params */
    if (SUCCESS != otpo_initialize_list (input_file, num_parameters))
    {
        exit (1);
    }

    /* fake MPI_init , does nothing*/
    MPI_Init (&argc, &argv);    
    ADCL_Init ();
    
    if (debug)
    {
        otpo_dump_list (num_parameters);       
    }
    
    /* allocating the list of ADCL attributes */
    ADCL_param_attributes = (ADCL_Attribute *)malloc(sizeof(ADCL_Attribute) * num_parameters);
    if( NULL == ADCL_param_attributes ) 
    {
        fprintf (stderr,"Malloc Failed...\n");
        exit (1);
    }
    
    if (SUCCESS != otpo_populate_attributes (num_parameters, ADCL_param_attributes))
    {
        exit(1);
    }
    ADCL_Attrset_create (num_parameters, ADCL_param_attributes, &ADCL_param_attrset);

    /* total number of combinations */
    num_comb = get_num_combinations (num_parameters);

    if (FAIL == (num_comb = otpo_populate_function_set (num_parameters, ADCL_param_attrset, 
                                                        num_comb, &ADCL_param_fnctset)))
    {
        exit(1);
    }

    if (debug) 
    {
        printf("Number of possible combinations: %d\n",num_comb);
    }

    ADCL_Topology_create_generic (0, NULL, NULL, NULL, ADCL_DIRECTION_BOTH,
                                  MPI_COMM_WORLD, &ADCL_param_topo);

    ADCL_Request_create (ADCL_VECTOR_NULL, ADCL_param_topo, ADCL_param_fnctset, 
                         &ADCL_param_request);

    signal(SIGINT, handler);
    signal(SIGHUP, handler);
    signal(SIGTERM, handler);

    /* Start the Tests */
    num_tested=0;
    if (1 == resume) 
    {
        printf("Reading data to Resume...\n");
        if (SUCCESS != otpo_read_interrupt_data (interrupt_file, &num_tested, &results))
        {
            fprintf (stderr, "Can't Restore Data\n");
            exit (1);
        }
        if (debug)
        {
            printf("%d combinations left\n", num_comb-num_tested);
        }
        unfiltered = (double *)malloc(sizeof(double)*num_tested);
        outliers = (double *)malloc(sizeof(double)*num_tested);
        if (NULL == unfiltered || NULL == outliers) 
        {
            fprintf (stderr,"Malloc Failed...\n");
            exit (1);
        }
        for (k=0 ; k<num_tested ; k++) 
        {
            unfiltered[k] = results[k];
            outliers[k] = 0.0;
        }
        ADCL_Request_restore_status (ADCL_param_request, num_tested,
                                     results, unfiltered, outliers);
        if (NULL != results)
        {
            free (results);
        }
        if (NULL != unfiltered)
        {
            free (unfiltered);
        }
        if (NULL != outliers)
        {
            free (outliers);
        }
    }
    current = (float)num_tested;
    if (resume)
    {
        current--;
    }
    /* we execute till num_comb + 1, because ADCL requires to execute one more 
       to switch into the decision state. The last function executed is the 
       winner function 
    */
    for (i=num_tested ; i<num_comb+1 ; i++) 
    {
        if (status || verbose || debug) 
        {
            if (i >= current)
            {
                printf ("Completed: %d%\r",(int)( (i/(float)num_comb) * 100 ));
                fflush(stdout);
                current += num_comb/100.0; 
            }
        }
        if (0 == stop_signal)
        {
            ADCL_Request_start (ADCL_param_request);
        }
        else if (1 == stop_signal && 1 < i)
        {
            printf ("Backing up Data...\n");
            ADCL_Request_save_status (ADCL_param_request, &num_tested,
                                      &results, &unfiltered, &outliers,
                                      &current_winner);

            if (SUCCESS != otpo_write_interrupt_data (num_tested, results, 
                                                      current_winner, interrupt_file))
            {
                fprintf(stderr,"Couldn't Backup Data, Quiting...\n");
                exit(1);
            }
            if (NULL != results)
            {
                free (results);
            }
            if (NULL != unfiltered)
            {
                free (unfiltered);
            }
            if (NULL != outliers)
            {
                free (outliers);
            }
            exit(1);
        }
        else if (1 == stop_signal)
        {
            exit(1);
        }
    }
    
    /* Output the results */
    if (SUCCESS != otpo_write_results (ADCL_param_request, output_dir, 
                                       &num_functions))
    {
        exit(1);
    }

    /* Output the results- seperate*/ 
    if (SUCCESS != otpo_analyze_results (output_dir, num_functions, 
                                         num_parameters))
    {
        exit(1);
    }
    
    ADCL_Request_free (&ADCL_param_request);
    ADCL_Topology_free (&ADCL_param_topo);
    ADCL_Fnctset_free (&ADCL_param_fnctset);
    ADCL_Attrset_free (&ADCL_param_attrset);
    for (i=0 ; i<num_parameters ; i++) 
    {
        ADCL_Attribute_free (&ADCL_param_attributes[i]);
    }
    ADCL_Finalize();
    MPI_Finalize();

    if (NULL != input_file) 
    {
        free (input_file);
    }
    if (NULL != output_dir) 
    {
        free (output_dir);
    }
    if (NULL != interrupt_file) 
    {
        free (interrupt_file);
    }
    otpo_free_list_params_objects(num_parameters);
    if (NULL != list_params) 
    {
        free (list_params);
    }
    if (NULL != test) 
    {
        free (test);
    }
    if (NULL != msg_size) 
    {
        free (msg_size);
    }
    for (i=0 ; i<mca_args_len ; i++)
    {   
        if (NULL != mca_args[i])
            free (mca_args[i]);
    }
    if (NULL != mca_args)
    { 
        free (mca_args);
    }
    if (NULL != hostf) 
    {
        free (hostf);
    }
    et2 = time (NULL);
    sec = 0;
    hr = 0;
    min = 0;
    sec = (int)difftime (et2, et1);
    if (3600 < sec)
    {
        hr = sec/3600;
        if (0 < sec%3600)
        {
            sec = sec - hr*3600 ;
        }
    }
    if (60 < sec)
    {
        min = sec/60;
        if (0 < sec%60)
        {
            sec = sec - min*60 ;
        }
    }
    printf ("Time Elapsed: %d hrs %d min %d sec\n", hr, min, (int)sec);
    return 0;
}

/* 
 * Handler function to detect signals 
 */
void handler (int sig)
{
    t2 = time (NULL);
    if (0 == stop_signal) 
    {
        printf ("Im in the Middle of Quiting, If you can't wait, Press Ctrl-C in the next SECOND to exit forcefully...\n");
        t1 = time (NULL);
        stop_signal = 1;
    }
    else if (1 == stop_signal && (t2-t1) < 1) 
    {
        exit (1);
    }
}

/*
 * Function to get the total number of combinations of attibute values
 */ 
static int get_num_combinations (int num_parameters)
{
    int i;
    int num;
    
    num = list_params[0].num_values;
    for (i=1 ; i<num_parameters ; i++)
    {
        num *= list_params[i].num_values;
    }

    return num;
}

/*
 * Function to set the mca options that the users provide for the mpirun command
 */
static int set_mca_options (int argc, char **argv) 
{
    int i,k;

    for (i=0 ; i<argc ; i++) 
    {
        if (0 == strcmp (argv[i],"--mca") || 0 == strcmp (argv[i], "-m"))
        {
            mca_args_len ++;
            while ('-' != argv[++i][0]) 
            {
                mca_args_len ++;
                if (argc == i+1) 
                    break;
            }
            i --;
        }
    }
    mca_args = (char **)malloc(sizeof(char *) * mca_args_len);
    if (NULL == mca_args) 
    {
        return NO_MEMORY;
    }
    k = 0;
    for (i=0 ; i<argc ; i++)
    {
        if (0 == strcmp (argv[i],"--mca") || 0 == strcmp (argv[i], "-m"))
        {
            mca_args[k++] = strdup("--mca");
            while ('-' != argv[++i][0]) 
            {
                mca_args[k++] = strdup(argv[i]);
                if (argc == i+1) 
                    break;
            }
            i --;
        }
    }

    return SUCCESS;
}

int otpo_dump_list (int num_parameters)
{
    int i,k;

    for (i=0 ; i<num_parameters ; i++) 
    {
        printf ("Name: %s\n",list_params[i].name);
        printf ("Number of Vales: %d\n", list_params[i].num_values);
        if (NULL != list_params[i].default_value) 
        {
            printf ("Default: %s\n",list_params[i].default_value);
        }
        for (k=0 ; k<list_params[i].num_values ; k++) 
        {
            if (NULL != list_params[i].string_values[k])
            {
                printf ("%d: %s\n",k,list_params[i].string_values[k]);
            }
            else 
            {
                printf ("%d: %d\n",k,list_params[i].possible_values[k]);
            }
        }
        if (NULL == list_params[i].string_values[0])
        {
            printf ("Start: %d\n",list_params[i].start_value);
            printf ("End: %d\n",list_params[i].end_value);
            printf ("Traversal method: %c\n",list_params[i].traverse);
            printf ("Operation: %c\n",list_params[i].operation);
            printf ("Increment: %d\n",list_params[i].traverse_attr.increment);
            if ('\0' != list_params[i].condition[0]) 
            {
                printf ("Condition: %s\n",list_params[i].condition);
            }
        }
        if (0 != list_params[i].num_rpn_elements)
        {
            printf ("RPN ELEMENTS:\t");
            for (k=0 ; k<list_params[i].num_rpn_elements ; k++)
            {
                if (list_params[i].rpn_elements[k].is_operator)
                {
                    printf("%c\t",list_params[i].rpn_elements[k].type.operator_type);
                }
                else
                {
                    switch (list_params[i].rpn_elements[k].type.operand_type)
                    {
                    case INTEGER:
                        printf("%d\t", list_params[i].rpn_elements[k].value.integer_value);
                        break;
                    case PARAM:
                        printf("%s\t", list_params[list_params[i].rpn_elements[k].value.param_index].name);
                        break;
                    }
                }
            }
        }
        printf ("\n");
        printf ("***********DONE*************\n"); 
    }
    return SUCCESS;
}

static void print_usage ()
{
    printf ("Usage: \n");
    printf ("-p InputFileName (file that contains the parameters)\n");
    printf ("-d (debug output)\n");
    printf ("-v (verbose output)\n");
    printf ("-s (status output)\n");
    printf ("-n (silent/no output)\n");
    printf ("-t test (name of test, Current supported: Netpipe)\n");
    printf ("-w test_path (path to the test on your system)\n");
    printf ("-l message_length\n");
    printf ("-h hostfile\n");
    printf ("-m mca_params (mca parameters that you want your mpirun to run with)\n");
    printf ("-f format (format of output, TXT)\n");
    printf ("-o output_dir\n");
    printf ("-b interrupt_file (file to write data to when interrupted)\n");
    printf ("-r interrupt_file (the file where contains the data to resume execution)\n");
}

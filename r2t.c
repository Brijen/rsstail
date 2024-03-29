#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <mrss.h>

void replace(char *in, char *what, char by_what)
{
	int what_len = strlen(what);

	/* replace &lt; etc. */
	for(;;)
	{
		char *str = strstr(in, what);
		if (!str)
			break;

		memcpy(str + 1, str + what_len, strlen(str + what_len) + 1);
		*str = by_what;
	}
}

char *remove_html_tags(char *in)
{
	char *copy = strdup(in);

	/* strip <...> */
	for(;;)
	{
		char *lt = strchr(copy, '<'), *gt;
		if (!lt)
			break;

		gt = strchr(lt, '>');
		if (!gt)
			break;

		memcpy(lt, gt + 1, strlen(gt + 1) + 1);
	}

	replace(copy, "&lt;", '<');
	replace(copy, "&gt;", '>');
	replace(copy, "&amp;", '&');

	return copy;
}

/* 0: is a new record, -1: not a new record */
int is_new_record(mrss_item_t *check_list, mrss_item_t *cur_item)
{
	while(check_list)
	{
		if (check_list -> pubDate != NULL && cur_item -> pubDate != NULL)
		{
			if (strcmp(check_list -> pubDate, cur_item -> pubDate) == 0)
				return -1;
		}
		else
		{
			int navail = 0, nequal = 0;

			if (check_list -> title != NULL && cur_item -> title != NULL)
			{
				navail++;

				if (strcmp(check_list -> title, cur_item -> title) == 0)
					nequal++;
			}
			if (check_list -> link  != NULL && cur_item -> link  != NULL)
			{
				navail++;

				if (strcmp(check_list -> link, cur_item -> link ) == 0)
					nequal++;
			}
			if (check_list -> description != NULL && cur_item -> description != NULL)
			{
				navail++;

				if (strcmp(check_list -> description, cur_item -> description) == 0)
					nequal++;
			}

			if (navail == nequal && navail > 0)
			{
				return -1;
			}
		}

		check_list = check_list -> next;
	}

	return 0;
}

void version(void)
{
	printf("rsstail v" VERSION ", (C) 2005-2007 by folkert@vanheusden.com\n\n");
}

void usage(void)
{
	version();

	printf("-t      show timestamp\n");
	printf("-l      show link\n");
	printf("-d      show description\n");
	printf("-p      show publication date\n");
	printf("-a      show author\n");
	printf("-c      show comments\n");
	printf("-N      do not show headings\n");
	printf("-b x	limit description/comments to x bytes\n");
	printf("-z      continue even if there are XML parser errors in the RSS feed\n");
	printf("-Z x    add heading 'x'\n");
	printf("-n x	initially show x items\n");
	printf("-r            reverse output, so it looks more like an RSS feed\n");
	printf("-H      strip HTML tags\n");
/*	printf("-o x    only show items newer then x[s/M/h/d/m/y]\n");	*/
	printf("-A x    authenticate against webserver, x is in format username:password\n");
	printf("-u url	URL of RSS feed to tail\n");
	printf("-i x	check interval in seconds (default is 15min)\n");
	printf("-x x	proxy server to use (host[:port])\n");
	printf("-y x	proxy authorization (user:password)\n");
	printf("-P      do not exit when an error occurs\n");
	printf("-1      one shot\n");
	printf("-v      be verbose (add more to be more verbose)\n");
	printf("-V      show version and exit\n");
	printf("-h      this help\n");
}

int main(int argc, char *argv[])
{
	char **url = NULL;
	int n_url = 0, cur_url = 0;
	int check_interval = 15 * 60;
	mrss_t **data_prev = NULL;
	mrss_t **data_cur = NULL;
	int data_size;
	char *proxy = NULL, *proxy_auth = NULL;
	int sw;
	int verbose = 0;
	char show_timestamp = 0, show_link = 0, show_description = 0, show_pubdate = 0, show_author = 0, show_comments = 0;
	char strip_html = 0, no_error_exit = 0;
	char one_shot = 0;
	char no_heading = 0;
	int bytes_limit = 0;
	time_t last_changed = (time_t)0;
	char continue_on_error = 0, dummy;
	int show_n = -1;
	long int max_age = -1;
	char *heading = NULL;
	mrss_options_t mot;
	char *auth = NULL;
	char reverse = 0;

	memset(&mot, 0x00, sizeof(mot));

	while((sw = getopt(argc, argv, "A:Z:1b:PHztldrpacu:Ni:n:x:y:vVh")) != -1)
	{
		switch(sw)
		{
			case 'A':
				auth = optarg;
				break;

			case 'Z':
				heading = optarg;
				break;

			case 'N':
				no_heading = 1;
				break;

			case '1':
				one_shot = 1;
				break;

			case 'b':
				bytes_limit = atoi(optarg);
				if (bytes_limit <= 0)
				{
					printf("-b requires a number > 0\n");
					return 1;
				}
				break;

			case 'P':
				no_error_exit = 1;
				break;

			case 'H':
				strip_html = 1;
				break;

			case 'n':
				show_n = atoi(optarg);
				if (show_n < 0)
				{
					printf("-n requires an positive value\n");
					return 1;
				}
				else if (show_n > 50)
					printf("Initially showing more then 50 items, must be one hell of an rss feed!\n");
				break;

#if 0
			case 'o':
				dummy = optarg[strlen(optarg) - 1];
				max_age = atoi(optarg);
				if (max_age < 0)
				{
					printf("-o requires an positive value\n");
					return 1;
				}
				if (dummy == 's')
					max_age *= 1;
				else if (dummy == 'M')
					max_age *= 60;
				else if (dummy == 'h')
					max_age *= 3600;
				else if (dummy == 'd')
					max_age *= 86400;
				else if (dummy == 'm')
					max_age *= 86400 * 31;
				else if (dummy == 'y')
					max_age *= 86400 * 365.25;
				else if (isalpha(dummy))
				{
					printf("'%c' is a not recognized multiplier\n", dummy);
					return 1;
				}
				break;
#endif

			case 'z':
				continue_on_error = 1;
				break;

			case 't':
				show_timestamp = 1;
				break;

			case 'l':
				show_link = 1;
				break;

			case 'd':
				show_description = 1;
				break;

			case 'r':
				reverse = 1;
				break;

			case 'p':
				show_pubdate = 1;
				break;

			case 'a':
				show_author = 1;
				break;

			case 'c':
				show_comments = 1;
				break;

			case 'u':
				url = (char **)realloc(url, sizeof(char *) * (n_url + 1));
				if (!url)
				{
					fprintf(stderr, "Cannot allocate memory\n");
					return 2;
				}
				url[n_url++] = optarg;
				break;

			case 'i':
				check_interval = atoi(optarg);
				break;

			case 'x':
				proxy = optarg;
				break;
			
			case 'y':
				proxy_auth = optarg;
				break;

			case 'v':
				verbose++;
				break;

			case 'V':
				version();
				return 1;

			case 'h':
			default:
				usage();
				return 1;
		}
	}

	mot.timeout = check_interval;
    	mot.proxy = proxy;
	mot.proxy_authentication = proxy_auth;
	mot.user_agent = "rsstail " VERSION ", (C) 2006-2013 by folkert@vanheusden.com";
	mot.authentication = auth;

	if (n_url == 0)
	{
		fprintf(stderr, "Please give the URL of the RSS feed to check with the '-u' parameter.\n");
		return 1;
	}

	data_size = sizeof(mrss_t *) * n_url;
	data_prev = (mrss_t **)malloc(data_size);
	data_cur  = (mrss_t **)malloc(data_size);
	if (!data_prev || !data_cur)
	{
		fprintf(stderr, "Cannot allocate memory\n");
		return 2;
	}

	memset(data_prev, 0x00, data_size);
	memset(data_cur , 0x00, data_size);

	if (verbose)
	{
		int loop;
		printf("Monitoring RSS feeds:\n");
		for(loop=0; loop<n_url; loop++)
			printf("\t%s\n", url[loop]);
		printf("Check interval: %d\n", check_interval);
	}

	for(;;)
	{
		mrss_error_t err_read;
		mrss_item_t *item_cur = NULL;
		mrss_item_t *first_item[n_url];
		mrss_item_t *tmp_first_item;
		time_t cur_last_changed;
		int n_shown = 0;

		if (verbose)
			printf("Retrieving RSS feed '%s'...\n", url[cur_url]);

		if ((err_read = mrss_get_last_modified_with_options(url[cur_url], &cur_last_changed, &mot)) != MRSS_OK)
		{
			if (err_read == MRSS_ERR_POSIX)
			{
				if (errno == EINPROGRESS)
				{
					fprintf(stderr, "Time-out while connecting to RSS feed, continuing\n");
					goto goto_next_url;
				}
			}

			fprintf(stderr, "Error reading RSS feed: %s\n", mrss_strerror(err_read));

			if (no_error_exit)
				goto goto_next_url;
			else
				return 2;
		}

		if (cur_last_changed == last_changed && cur_last_changed != 0)
		{
			if (verbose)
				printf("Feed did not change since last check.\n");
			goto goto_next_url;
		}
		else if (verbose > 2)
		{
			printf("Feed change detected, %s", ctime(&cur_last_changed));
		}

		last_changed = cur_last_changed;

		if ((err_read = mrss_parse_url_with_options(url[cur_url], &data_cur[cur_url], &mot)) != MRSS_OK)
		{
			if (err_read == MRSS_ERR_POSIX)
			{
				if (errno == EINPROGRESS)
				{
					fprintf(stderr, "Time-out while connecting to RSS feed, continuing\n");
					goto goto_next_url;
				}
			}
			else if (err_read == MRSS_ERR_PARSER && continue_on_error)
			{
				fprintf(stderr, "Error reading RSS feed: %s\n", mrss_strerror(err_read));
				goto goto_next_url;
			}

			fprintf(stderr, "Error reading RSS feed: %s\n", mrss_strerror(err_read));
			if (no_error_exit)
				goto goto_next_url;
			else
				return 2;
		}

		item_cur = data_cur[cur_url] -> item;

		if (reverse) {
			if (verbose)
			       printf("Reversing...\n");
			mrss_item_t *rev_item_cur = NULL;
			mrss_item_t *rev_item_last = NULL;
			for (;;) {
			       rev_item_last = rev_item_cur;
			       rev_item_cur = item_cur;
			       if (item_cur -> next) {
				       item_cur = item_cur -> next;
				       rev_item_cur -> next = rev_item_last;
			       }
			       else {
				       rev_item_cur -> next = rev_item_last;
				       break;
			       }
			}
		}

		tmp_first_item = item_cur;

		while(item_cur)
		{
			if ((data_prev[cur_url] && is_new_record(first_item[cur_url], item_cur) != -1) ||
			    (!data_prev[cur_url]))
			{
#if 0
				if (/* pubdate */ < max_age && max_age != -1)
					continue;
#endif

				if ((!data_prev[cur_url]) && n_shown >= show_n && show_n != -1)
				{
					item_cur = item_cur -> next;
					continue;
				}
				n_shown++;

				if ((show_link + show_description + show_pubdate + show_author + show_comments ) > 1)
					printf("\n");

				if (show_timestamp)
				{
					time_t now = time(NULL);
					struct tm *now_tm = localtime(&now);

					printf("%04d/%02d/%02d %02d:%02d:%02d  ",
							now_tm -> tm_year + 1900,
							now_tm -> tm_mon + 1,
							now_tm -> tm_mday,
							now_tm -> tm_hour,
							now_tm -> tm_min,
							now_tm -> tm_sec);
				}

				if (heading)
				{
					printf(" %s", heading);
				}

				if (item_cur -> title != NULL)
					printf("%s%s\n", no_heading?" ":"Title: ", item_cur -> title);

				if (show_link && item_cur -> link != NULL)
					printf("%s%s\n", no_heading?" ":"Link: ", item_cur -> link);

				if (show_description && item_cur -> description != NULL)
				{
					if (strip_html)
					{
						char *stripped = remove_html_tags(item_cur -> description);

						if (bytes_limit != 0 && bytes_limit < strlen(stripped))
							stripped[bytes_limit] = 0x00;

						printf("%s%s\n", no_heading?" ":"Description: ", stripped);

						free(stripped);
					}
					else
					{
						if (bytes_limit != 0 && bytes_limit < strlen(item_cur -> description))
							(item_cur -> description)[bytes_limit] = 0x00;

						printf("%s%s\n", no_heading?" ":"Description: ", item_cur -> description);
					}
				}

				if (show_pubdate && item_cur -> pubDate != NULL)
					printf("%s%s\n", no_heading?" ":"Pub.date: ", item_cur -> pubDate);

				if (show_author && item_cur -> author != NULL)
					printf("%s%s\n", no_heading?" ":"Author: ", item_cur -> author);

				if (show_comments && item_cur -> comments != NULL)
				{
					if (bytes_limit != 0 && bytes_limit < strlen(item_cur -> comments))
						(item_cur -> comments)[bytes_limit] = 0x00;

					printf("%s%s\n", no_heading?" ":"Comments: ", item_cur -> comments);
				}
			}

			item_cur = item_cur -> next;
		}

		if (data_prev[cur_url])
		{
			mrss_error_t err_free = mrss_free(data_prev[cur_url]);

			if (err_free != MRSS_OK)
			{
				fprintf(stderr, "Error freeing up memory: %s\n", mrss_strerror(err_read));
				if (no_error_exit)
					goto goto_next_url;
				else
					return 2;
			}
		}

		data_prev[cur_url] = data_cur[cur_url];
		data_cur[cur_url] = NULL;
		first_item[cur_url] = tmp_first_item;

goto_next_url:
		cur_url++;
		if (cur_url == n_url)
			cur_url = 0;

		fflush(stdout);

		if (one_shot)
			break;

		if (verbose > 2)
			printf("Sleeping...\n");
		sleep(check_interval / n_url);
	}

	return 0;
}

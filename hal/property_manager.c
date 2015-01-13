//
// Copyright 2014 Per Vices Corporation
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "common.h"
#include "properties.h"
#include "comm_manager.h"
#include "property_manager.h"

#define EVENT_SIZE 	(sizeof(struct inotify_event))
#define EVENT_BUF_LEN 	( 1024 * (EVENT_SIZE + 16) )

// UART communication manager's file descriptor
static int uart_comm_fd;

// Inotify's file descriptor
static int inotify_fd;

// Helper function to write to property
static void write_to_file(const char* path, const char* data) {
	FILE* fd;
	if ( !(fd = fopen(path, "w")) ) {
		fprintf(stderr, "%s(): ERROR, %s\n", __func__, strerror(errno));
		return;
	}
	fprintf(fd, "%s", data);
	fclose(fd);

	#ifdef DEBUG
	printf("wrote to file: %s (%s)\n", path, data);
	#endif
}

// Helper function to read to property
static void read_from_file(const char* path, char* data, size_t max_len) {
	FILE* fd;
	if ( !(fd = fopen(path, "r")) ) {
		fprintf(stderr, "%s(): ERROR, %s\n", __func__, strerror(errno));
		return;
	}
	fgets(data, max_len, fd);
	fclose(fd);
	
	// remove the new line at the end of the file
	size_t pos = 0;
	while(data[pos] != '\n' && data[pos] != '\0') pos++;
	data[pos] = '\0';

	#ifdef DEBUG
	printf("read from file: %s (%s)\n", path, data);
	#endif
}

// Helper function to make properties
static inline void make_prop(prop_t* prop) {
	char cmd [MAX_PATH_LEN];
	char path[MAX_PATH_LEN];

	// mkdir -p /home/root/state/*
	strcpy(cmd, "mkdir -p ");
	strcat(cmd, get_abs_dir(prop, path));
	system(cmd);
	//printf("executing: %s\n", cmd);

	// touch /home/root/state/*
	strcpy(cmd, "touch ");
	strcat(cmd, get_abs_path(prop, path));
	system(cmd);
	//printf("executing: %s\n", cmd);

	// if read only property, change permissions
	if (prop -> permissions == RO) {
		// chmod a-w /home/root/state/*
		strcpy(cmd, "chmod 0444 ");
		strcat(cmd, get_abs_path(prop, path));
		system(cmd);
	} else if (prop -> permissions == WO) {
		// chmod a-r /home/root/state/*
		strcpy(cmd, "chmod 0222 ");
		strcat(cmd, get_abs_path(prop, path));
		system(cmd);
	}
}

// Helper function to add the property to inotify
// crimson continues if this fails, but will give an error
static void add_prop_to_inotify(prop_t* prop) {
	char path[MAX_PATH_LEN];

	// check if RO property
	if (prop -> permissions != RO) {
		prop -> wd = inotify_add_watch( inotify_fd,
			get_abs_path(prop, path), IN_CLOSE_WRITE);
	}

	if (prop -> wd < 0)
		fprintf(stderr, "%s(): ERROR, %s\n", __func__, strerror(errno));
}

// Helper function to call power-on reset values
static void init_prop_val(prop_t* prop) {
	char path [MAX_PATH_LEN];
	memset(path, 0, MAX_PATH_LEN);

	// if not WO property
	if  (prop -> permissions != WO) {
		write_to_file(get_abs_path(prop, path), prop -> def_val);
	}
}

// Helper function for building a tree in the home directory
static void build_tree(void) {
	#ifdef DEBUG
	printf("Building tree, %i properties found\n", get_num_prop());
	#endif

	size_t i;
	for (i = 0; i < get_num_prop(); i++) {
		make_prop(get_prop(i));
		init_prop_val(get_prop(i));
		add_prop_to_inotify(get_prop(i));
	}

	#ifdef DEBUG
	printf("Last wd val: %i\n", get_prop(i-1) -> wd);
	printf("Done building tree\n");
	#endif
}

// Initialize handler functions
int init_property(void) {
	// uart transactions
	#ifdef DEBUG
	printf("Initializing UART\n");
	#endif

	if ( init_uart_comm(&uart_comm_fd, UART_DEV, 0) < 0 ) {
		printf("ERROR: %s, cannot initialize uart %s\n", __func__, UART_DEV);
		return RETURN_ERROR_COMM_INIT;
	}

	#ifdef DEBUG
	printf("init_uart_comm(): UART connection up\n");
	printf("Initializing Inotify\n");
	#endif

	// inotify
	if ( (inotify_fd = inotify_init()) < 0) {
		printf("ERROR: %s, cannot initialize inotify\n", __func__);
		return RETURN_ERROR_INOTIFY;
	}

	// set inotify to non-blocking
	fcntl(inotify_fd, F_SETFL, fcntl(inotify_fd, F_GETFL) | O_NONBLOCK);

	// pass the uart handler to the property handlers
	pass_uart_fd(uart_comm_fd);

	build_tree();
	return RETURN_SUCCESS;
}

// non-standard set property (file modification)
void check_property_inotifies(void) {
	uint8_t buf[EVENT_BUF_LEN];
	char prop_data[MAX_PROP_LEN];
	char prop_ret[MAX_PROP_LEN];
	char path[MAX_PATH_LEN];
	ssize_t len = read(inotify_fd, buf, EVENT_BUF_LEN);

	ssize_t i = 0;
	while (i < len) {
		// gets the event structure
		struct inotify_event* event = (struct inotify_event*) &buf[i];
		prop_t* prop = get_prop_from_wd(event -> wd);

		if (event -> mask & IN_CLOSE_WRITE) {
			#ifdef DEBUG
			printf("Property located at %s has been modified, executing handler\n", prop -> path);
			#endif

			// empty out the buffers
			memset(prop_data, 0, MAX_PROP_LEN);
			memset(prop_ret,  0, MAX_PROP_LEN);

			// read the change from the file
			read_from_file(get_abs_path(prop, path), prop_data, MAX_PROP_LEN);
			strcpy(prop_ret, prop_data);
			prop -> set_handler(prop_data, prop_ret);

			// if the return value didn't change, don't write to file again
			if ( strcmp(prop_ret, prop_data) != 0) {
				// temperarily remove property from inotify so the file update won't trigger another inotify event
				if (inotify_rm_watch( inotify_fd, prop -> wd) < 0)
					fprintf(stderr, "%s(): ERROR, %s\n", __func__, strerror(errno));
				#ifdef DEBUG
				printf("Removed inotify, wd: %i\n", prop -> wd);
				#endif

				// write output of set_handler to property
				write_to_file(get_abs_path(prop, path), prop_ret);

				// re-add property to inotify
				prop -> wd = inotify_add_watch( inotify_fd, get_abs_path(prop, path), IN_CLOSE_WRITE);
				if (prop -> wd < 0)
					fprintf(stderr, "%s(): ERROR, %s\n", __func__, strerror(errno));
				#ifdef DEBUG
				printf("Re-added to inotify, wd: %i\n", prop -> wd);
				#endif
			}
		}

		i += sizeof(struct inotify_event) + event -> len;
	}
}

// Standard get property
int get_property(const char* prop, char* data, size_t max_len) {
	#ifdef DEBUG
	printf("%s(): %s\n", __func__, prop);
	#endif

	memset(data, 0, max_len);
	char path [MAX_PATH_LEN];
	prop_t* temp = get_prop_from_cmd(prop);

	// check if valid property
	if (!temp) {
		printf("Property: %s does not exist\n", prop);
		return RETURN_ERROR_SET_PROP;		
	}

	// check if WO property
	if (temp -> permissions == WO) {
		printf("Cannot invoke a get on this property\n");
		return RETURN_ERROR_GET_PROP;
	}

	read_from_file(get_abs_path(temp, path), data, max_len);
	return RETURN_SUCCESS;
}

// standard set property
int set_property(const char* prop, const char* data) {
	#ifdef DEBUG
	printf("%s(): %s\n", __func__, prop);
	#endif

	char path [MAX_PATH_LEN];
	prop_t* temp = get_prop_from_cmd(prop);

	// check if valid property
	if (!temp) {
		printf("Property: %s does not exist\n", prop);
		return RETURN_ERROR_SET_PROP;		
	}

	// check if RO property
	if (temp -> permissions == RO) {
		printf("Cannot invoke a set on this property\n");
		return RETURN_ERROR_SET_PROP;
	}

	write_to_file(get_abs_path(temp, path), data);
	check_property_inotifies();
	return RETURN_SUCCESS;
}

// For polling properties
void update_status_properties(void) {
	char buf[MAX_PROP_LEN];
	char path [MAX_PATH_LEN];
	size_t i;

	// check if continue polling
	get_property("poll_en", buf, MAX_PROP_LEN);
	if (buf[0] != '1') return;

	for (i = 0; i < get_num_prop(); i++) {
		// if POLL and RO property
		if  (get_prop(i) -> poll == POLL) {
			get_prop(i) -> get_handler(buf, buf);
			write_to_file(get_abs_path(get_prop(i), path), buf);
		}
	}
}

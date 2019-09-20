new Vue({
    el: '#app',
    data () {
	return {
	    local_info: null,
	    local_config: null
	}
    },
    filters: {
    },
    mounted () {
	axios
	    .get('/api/local.json')
	    .then(response => {
		this.local_info = response.data
	    })
	    .catch(error => {
		console.log(error)
	    });
	axios
	    .get('/api/local-config.json')
	    .then(response => {
		this.local_config = response.data
	    })
	    .catch(error => {
		console.log(error)
	    });
    }
})

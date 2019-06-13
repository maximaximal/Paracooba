pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    dir('paracuber') {
	stages {
	    stage('Build') {
		steps [
		    sh 'cmake . -G "Ninja" -B./build'
		    sh 'cd build && ninja'
		]
	    }
	}	
    }
}
